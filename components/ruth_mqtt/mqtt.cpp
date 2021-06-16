/*
     mqtt.cpp - Ruth MQTT
     Copyright (C) 2017  Tim Hughey

     This program is free software: you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation, either version 3 of the License, or
     (at your option) any later version.

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with this program.  If not, see <http://www.gnu.org/licenses/>.

     https://www.wisslanding.com
 */

#include <algorithm>
#include <cstdlib>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "boot.hpp"

// #include "core/core.hpp"
// #include "core/ota.hpp"
// #include "engines/ds.hpp"
// #include "engines/i2c.hpp"
// #include "engines/pwm.hpp"
// #include "local/types.hpp"
// #include "misc/restart.hpp"
#include "mqtt.hpp"
#include "status_led.hpp"

namespace ruth {

using namespace std;
using namespace message;

// using TR = ruth::Text_t;

static const char *TAG = "Rmqtt";
static const char *ESP_TAG = "ESP-MQTT";

static MQTT *_instance_ = nullptr;

MQTT::MQTT(const Opts &opts) : _rpt_filter('r'), _host_filter() {

  _host_filter.appendMultiLevelWildcard();

  esp_log_level_set(TAG, ESP_LOG_INFO);
  esp_mqtt_client_config_t client_opts = {};

  client_opts.uri = opts.uri;
  client_opts.disable_clean_session = false;
  client_opts.username = opts.user;
  client_opts.password = opts.passwd;
  client_opts.client_id = opts.client_id;
  client_opts.reconnect_timeout_ms = 3000;
  // priority of the ESP MQTT task responsible for sending and receiving messages
  client_opts.task_prio = 11;
  client_opts.buffer_size = 1024;
  client_opts.out_buffer_size = 5120;
  client_opts.user_context = this;

  _connection = esp_mqtt_client_init(&client_opts);
  esp_mqtt_client_register_event(_connection, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, eventHandler,
                                 _connection);

  esp_mqtt_client_start(_connection);
}

MQTT::~MQTT() {
  // memory clean up handled by shutdown
}

void MQTT::announceStartup() {
  message::Boot msg;
  send(msg);

  StatusLED::off();
}

void MQTT::connectionClosed() {
  StatusLED::dim();

  _mqtt_ready = false;
}

void MQTT::core(void *data) {
  esp_mqtt_client_start(_connection);

  // core forever loop
  for (auto announce = true; _run_core;) {
    if (_mqtt_ready && announce) {
      announceStartup();
      announce = false;

      UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(nullptr);
      auto stack_percent = 100.0 - ((float)stack_high_water / (float)_task.stackSize * 100.0);

      ESP_LOGV(TAG, "reporting to filter[%s]", _rpt_filter.c_str());
      ESP_LOGI(TAG, "after announce stack used[%0.1f%%] hw[%u]", stack_percent, stack_high_water);
    }

    vTaskDelay(1000);
  }

  // if the core task loop ever exits then a shutdown is underway.

  // a. signal mqtt is no longer ready
  // b. destroy (free) the client control structure
  _mqtt_ready = false;
  esp_mqtt_client_destroy(_connection);
  _connection = nullptr;

  // wait here forever, vTaskDelete will remove us from the scheduler
  vTaskDelay(UINT32_MAX);
}

void MQTT::coreTask(void *task_instance) {
  MQTT *mqtt = _instance_;
  Task_t *task = &(mqtt->_task);

  esp_register_shutdown_handler(MQTT::shutdown);

  mqtt->core(task->data);

  // if the core task ever returns then wait forever
  vTaskDelay(UINT32_MAX);
}

IRAM_ATTR esp_err_t MQTT::eventCallback(esp_mqtt_event_handle_t event) {
  esp_err_t rc = ESP_OK;
  esp_mqtt_client_handle_t client = event->client;
  esp_mqtt_connect_return_code_t status;

  MQTT *mqtt = (MQTT *)event->user_context;

  switch (event->event_id) {
  case MQTT_EVENT_BEFORE_CONNECT:
    StatusLED::brighter();
    break;

  case MQTT_EVENT_CONNECTED:
    status = event->error_handle->connect_return_code;
    ESP_LOGI(ESP_TAG, "CONNECT msg[%p] err_code[%d]", (void *)event, status);

    if (status != MQTT_CONNECTION_ACCEPTED) {
      ESP_LOGW(ESP_TAG, "mqtt connection error[%d]", status);
      rc = ESP_FAIL;
    }

    StatusLED::off();
    mqtt->subscribe(client);

    break;

  case MQTT_EVENT_DISCONNECTED:
    StatusLED::dim();

    mqtt->connectionClosed();
    break;

  case MQTT_EVENT_SUBSCRIBED:
    mqtt->subscribeAck(event);
    break;

  case MQTT_EVENT_DATA:
    mqtt->incomingMsg(event);
    break;

  case MQTT_EVENT_PUBLISHED:
    mqtt->brokerAck();
    break;

  case MQTT_EVENT_ERROR:
    mqtt->_last_return_code = event->error_handle->connect_return_code;
    break;

  default:
    ESP_LOGW(ESP_TAG, "unhandled event[0x%04x]", event->event_id);
    break;
  }

  return rc;
}

IRAM_ATTR void MQTT::eventHandler(void *handler_args, esp_event_base_t base, int32_t event_id,
                                  void *event_data) {
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

  eventCallback(event);
}

IRAM_ATTR void MQTT::incomingMsg(esp_mqtt_event_t *event) {

  // ensure there is actually a payload to handle
  if (event->total_data_len == 0) return;

  InWrapped msg = In::make(event->topic, event->topic_len, event->data, event->data_len);

  bool wanted = false;
  for (auto i = 0; (i < _max_handlers) && !wanted; i++) {
    const RegisteredHandler &registered = _handlers[i];

    if (registered.handler && registered.matchCategory(msg->category())) {
      registered.handler->wantMessage(msg);

      if (msg->wanted()) {
        wanted = true;

        registered.handler->accept(std::move(msg));
      }
    }
  }

  if (!wanted) {
    ESP_LOGW(TAG, "unwanted msg: %s", event->topic);
  }
}

void MQTT::registerHandler(message::Handler *handler, std::string_view category) {

  auto handler_list = _instance_->_handlers;

  for (auto k = 0; k < _instance_->_max_handlers; k++) {
    if (handler_list[k].handler == nullptr) {
      handler_list[k].handler = handler;
      category.copy(handler_list[k].category, sizeof(handler_list[k].category));
      break;
    }
  }
}

void MQTT::shutdown() {
  // make a copy of the instance pointer to delete
  MQTT *mqtt = _instance_;

  // grab the task handle for the vTaskDelete after initial shutdown steps
  TaskHandle_t handle = mqtt->_task.handle;

  // set __singleton__ to nullptr so any calls from other tasks will
  // quietly return
  _instance_ = nullptr;

  // flag that a shutdown is underway so the actual MQTT task will exit it's
  // core "forever" loop
  mqtt->_run_core = false;

  // poll the MQTT task until it indicates that it is no longer ready
  // and can be safely removed from the scheduler and deleted
  while (mqtt->_mqtt_ready) {
    // delay the CALLING task
    // the MQTT shutdown activities should be "quick"
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // ok, MQTT task has cleanly closed it's connections

  // vTaskDelete removes the task from the scheduler.
  // so we must call mg_mgr_free() before

  // a. signal FreeRTOS to remove the task from the scheduler
  // b. the IDLE task will ultimately free the FreeRTOS memory for the task.
  // the memory allocated by the MQTT instance must be freed in it's
  // destructor.
  vTaskDelete(handle);
}

// void MQTT::start(const Opts &opts) {
//   _instance_ = new MQTT(opts);
//   MQTT *mqtt = _instance_;
//
//   mqtt->_host_filter.appendMultiLevelWildcard();
// }

void MQTT::start(const Opts &opts) {
  _instance_ = new MQTT(opts);
  MQTT *mqtt = _instance_;

  Task_t *task = &(mqtt->_task);

  ::xTaskCreate(&coreTask, TAG, task->stackSize, mqtt, task->priority, &(task->handle));
}

void MQTT::subscribeAck(esp_mqtt_event_handle_t event) {
  UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(nullptr);

  if (event->msg_id == _subscribe_msg_id) {
    auto stack_size = 6 * 1024;
    auto stack_percent = 100.0 - ((float)stack_high_water / (float)stack_size * 100.0);

    ESP_LOGI(ESP_TAG, "stack[%0.1f%%] SUBSCRIBED msg_id[%u]", stack_percent, event->msg_id);

    _mqtt_ready = true;
    // NOTE: do not announce startup here.  doing so creates a race condition
    // that results in occasionally using epoch as the startup time
  } else {
    ESP_LOGW(TAG, "subscribeAck for UNKNOWN msg_id[%d]", event->msg_id);
  }
}

void MQTT::subscribe(esp_mqtt_client_handle_t client) {
  const char *topic = _host_filter.c_str();
  int qos = 1; // hardcoded QoS

  _subscribe_msg_id = esp_mqtt_client_subscribe(client, topic, qos);

  ESP_LOGI(TAG, "subscribe to filter[%s] msg_id[%d]", topic, _subscribe_msg_id);
}

IRAM_ATTR TaskHandle_t MQTT::taskHandle() { return _instance_->_task.handle; }

IRAM_ATTR bool MQTT::send(message::Out &msg) {
  MQTT *mqtt = _instance_;

  if (mqtt == nullptr) return false;

  auto conn = mqtt->_connection;
  size_t bytes;
  auto packed = msg.pack(bytes);

  auto msg_id = esp_mqtt_client_publish(conn, msg.filter(), packed.get(), bytes, msg.qos(), false);

  // esp_mqtt_client_publish returns the msg_id on success, -1 if failed
  return (msg_id >= 0) ? true : false;
}

} // namespace ruth
