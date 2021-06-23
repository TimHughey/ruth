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

// override component logging level (must be #define before including esp_log.h)
// #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// #include "misc/status_led.hpp"
#include "mqtt.hpp"

namespace ruth {

using namespace std;
using namespace message;

static const char *TAG = "Rmqtt";
static const char *ESP_TAG = "ESP-MQTT";

static MQTT __singleton__;

void MQTT::connectionClosed() {
  // StatusLED::dim();

  _mqtt_ready = false;
}

IRAM_ATTR esp_err_t MQTT::eventCallback(esp_mqtt_event_handle_t event) {
  esp_err_t rc = ESP_OK;
  esp_mqtt_connect_return_code_t status;

  MQTT *mqtt = (MQTT *)event->user_context;

  switch (event->event_id) {
  case MQTT_EVENT_BEFORE_CONNECT:
    // StatusLED::brighter();
    break;

  case MQTT_EVENT_CONNECTED:
    status = event->error_handle->connect_return_code;
    ESP_LOGD(ESP_TAG, "CONNECT err_code[%d]", status);

    if (status == MQTT_CONNECTION_ACCEPTED) {
      const ConnOpts &opts = mqtt->_opts;

      // StatusLED::off();
      xTaskNotify(opts.notify_task, MQTT::CONNECTED, eSetBits);
    } else {

      ESP_LOGW(ESP_TAG, "mqtt connection error[%d]", status);
      rc = ESP_FAIL;
    }

    break;

  case MQTT_EVENT_DISCONNECTED:
    // StatusLED::dim();

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

    if (registered.handler == nullptr) break; // stop checking once we hit an empty registration

    if (registered.matchCategory(msg->category())) {
      registered.handler->wantMessage(msg);

      if (msg->wanted()) {
        wanted = true;

        registered.handler->accept(std::move(msg));

        if (registered.notify_task) {
          // notify the registered handler but DO NOT overwrite a pending notification
          xTaskNotify(registered.notify_task, MQTT::QUEUED_MSG, eSetValueWithoutOverwrite);
        }
      }
    }
  }

  if (!wanted) {
    ESP_LOGW(TAG, "unwanted msg: %s", event->topic);
  }
}

void MQTT::initAndStart(const ConnOpts &opts) {
  auto &mqtt = __singleton__;

  mqtt._opts = opts;

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
  client_opts.user_context = &__singleton__;

  auto &connection = mqtt._connection;

  connection = esp_mqtt_client_init(&client_opts);
  esp_mqtt_client_register_event(connection, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, eventHandler, connection);

  esp_mqtt_client_start(connection);
  // auto &task = mqtt._task;
  //
  // ::xTaskCreate(&coreTask, TAG, task.stackSize, &mqtt, task.priority, &(task.handle));
}

void MQTT::registerHandler(message::Handler *handler, std::string_view category) {
  auto &mqtt = __singleton__;

  auto &handler_list = mqtt._handlers;
  auto &max_handlers = mqtt._max_handlers;

  for (auto k = 0; k < max_handlers; k++) {
    if (handler_list[k].handler == nullptr) {
      handler_list[k].handler = handler;
      category.copy(handler_list[k].category, sizeof(handler_list[k].category));
      break;
    }
  }
}

void MQTT::subscribeAck(esp_mqtt_event_handle_t event) {

  if (event->msg_id == _subscribe_msg_id) {
    _mqtt_ready = true;

    // notify the awaiting task
    auto *mqtt = (MQTT *)event->user_context;
    auto const &opts = mqtt->_opts;
    xTaskNotify(opts.notify_task, MQTT::READY, eSetBits);

#ifdef LOG_LOCAL_LEVEL
    constexpr auto stack = CONFIG_MQTT_TASK_STACK_SIZE;
    const auto high_water = uxTaskGetStackHighWaterMark(nullptr);

    ESP_LOGD(ESP_TAG, "SUBSCRIBE ACK msg_id[%u]", event->msg_id);
    ESP_LOGD(ESP_TAG, "MQTT READY stack[%u] highwater[%u]", stack, high_water);
#endif

    // NOTE: do not announce startup here.  doing so creates a race condition
    // that results in occasionally using epoch as the startup time
  } else {
    ESP_LOGW(TAG, "SUBSCRIBE ACK for UNKNOWN msg_id[%d]", event->msg_id);
  }
}

void MQTT::subscribe(const filter::Subscribe &filter) {
  auto &connection = __singleton__._connection;

  int qos = 1; // hardcoded QoS

  auto &sub_msg_id = __singleton__._subscribe_msg_id;
  sub_msg_id = esp_mqtt_client_subscribe(connection, filter.c_str(), qos);

  ESP_LOGD(TAG, "SUBSCRIBE TO filter[%s] msg_id[%d]", filter.c_str(), sub_msg_id);
}

IRAM_ATTR bool MQTT::send(message::Out &msg) {
  auto &mqtt = __singleton__;

  if (mqtt._mqtt_ready == false) return false;

  auto &conn = mqtt._connection;
  size_t bytes;
  auto packed = msg.pack(bytes);

  auto msg_id = esp_mqtt_client_publish(conn, msg.filter(), packed.get(), bytes, msg.qos(), false);

  // esp_mqtt_client_publish returns the msg_id on success, -1 if failed
  return (msg_id >= 0) ? true : false;
}

} // namespace ruth
