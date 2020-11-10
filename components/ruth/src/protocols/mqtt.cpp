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
#include <array>
#include <cstdlib>
#include <memory>
#include <string>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "core/core.hpp"
#include "core/ota.hpp"
#include "engines/ds.hpp"
#include "engines/i2c.hpp"
#include "engines/pwm.hpp"
#include "local/types.hpp"
#include "misc/restart.hpp"
#include "misc/status_led.hpp"
#include "net/network.hpp"
#include "net/profile/profile.hpp"
#include "protocols/mqtt.hpp"
#include "readings/readings.hpp"

namespace ruth {

using std::max;
using std::move;
using std::unique_ptr;

using TR = ruth::Text_t;

static const char *TAG = "MQTT";
static const char *ESP_TAG = "ESP-MQTT";
// __singleton__ is used by private MQTT static functions
static MQTT *__singleton__ = nullptr;
// _instance_ is used for public API
static MQTT *_instance_ = nullptr;

// SINGLETON! constructor is private
MQTT::MQTT() {
  // create the report and command feed topics
  _feed_prefix = Binder::env();
  _feed_prefix.push_back('/');
  _feed_rpt = _feed_prefix + _feed_rpt_prefix + Net::hostID();
  _feed_host = _feed_prefix + Net::hostID() + _feed_host_suffix;

  ESP_LOGV(TAG, "reporting to feed[%s]", _feed_rpt.c_str());
}

MQTT::~MQTT() { // memory clean up handled by shutdown
}

void MQTT::announceStartup() {
  uint32_t batt_mv = Core::batteryMilliVolt();
  Startup_t startup(batt_mv);

  publish(&startup);
  StatusLED::off();
}

void MQTT::connectionClosed() {
  StatusLED::dim();

  _mqtt_ready = false;

  Net::clearTransportReady();
}

bool MQTT::handlePayload(MsgPayload_t_ptr payload_ptr) {
  auto payload_rc = false;
  auto payload = payload_ptr.get();
  // save a copy of the subtopic in case we need to report a failure
  // and the original payload has been moved and possibly freed
  string_t subtopic(payload->subtopic());

  if (payload->invalid()) {
    TR::rlog("[MQTT] invalid topic[%s]", payload->errorTopic());
  }

  // NOTE:
  // in the various cases below we either move the payload_ptr to another
  // function or directly use the contents.  once this if/then/else completes
  // it is UNSAFE to use the payload_ptr -- assume it is no longer valid.
  // if the payload_ptr happens to still hold a valid pointer, once it falls
  // out of scope it will be freed.

  // only handle Engine commands if the Engines have been started
  if (Core::enginesStarted()) {
    if (payload->matchSubtopic("pwm")) {
      payload_rc = PulseWidth::queuePayload(move(payload_ptr));

    } else if (payload->matchSubtopic("i2c")) {
      payload_rc = I2c::queuePayload(move(payload_ptr));

    } else if (payload->matchSubtopic("ds")) {
      payload_rc = DallasSemi::queuePayload(move(payload_ptr));
    }
  }

  // always handle Profile, OTA and Restart messages regardless of if
  // Engines (e.g. PulseWidth, DalSemi, I2c) are running.  this covers
  // the typically short period of time between assignment of an
  // IP, SNTP completion, announcing startup and receipt of the Profile
  // and start of the Engines.

  // said differently, Engine commands could be received prior to
  // the Profile which can not be processed and must be ignored.

  // we can also process OTA and restart commands
  if (payload->matchSubtopic("profile")) {
    Profile::fromRaw(payload);

    if (Profile::valid()) {
      payload_rc = Profile::postParseActions();
    }
  } else if (payload->matchSubtopic("ota")) {
    OTA *ota = OTA::payload(move(payload_ptr));
    payload_rc = Core::otaRequest(ota);

  } else if (payload->matchSubtopic("restart")) {
    payload_rc = Core::restartRequest();
  }

  if (payload_rc == false) {
    TR::rlog("[MQTT] subtopic[%s] failed", subtopic.c_str());
  }

  return payload_rc;
}

void MQTT::incomingMsg(esp_mqtt_event_t *event) {

  // ensure there is actually a payload to handle
  if (event->total_data_len == 0)
    return;

  // we wrap the newly allocated MsgPayload in a unique_ptr then move
  // it downstream to the various classes that process it.  the only potential
  // memory leak is when the payload is queued to an Engine.
  MsgPayload_t_ptr payload_ptr(new MsgPayload(event));

  // messages with subtopics are handled locally
  //  1. placed on a queue for another task
  //  2. if minimal processing required then do it in this task

  // EXAMPLES:
  //   prod/ruth.mac_addr/profile
  //   prod/ruth.max_addr/ota
  //   prod/ruth.max_addr/restart
  //   prod/ruth.max_addr/ds
  //   prod/ruth.max_addr/pwm
  //   prod/ruth.max_addr/i2c

  // NOTE:  if a downstream object queues the actual payload then it must
  //        free the memory when finished
  handlePayload(move(payload_ptr));
}

void MQTT::publishMsg(string_t *msg) {
  if (_connection) {

    _msg_id = esp_mqtt_client_publish(_connection, _feed_rpt.c_str(),
                                      msg->data(), msg->size(), 0, false);

    // esp_mqtt_client_publish returns the msg_id on success, -1 if failed
    if (_msg_id < 0) {
      // pass a nullptr for the message so Restart doesn't attempt to publish
      Restart::restart(nullptr, __PRETTY_FUNCTION__);
    }

    _msg_max_size = max(msg->size(), _msg_max_size);
  }

  delete msg;
}

void MQTT::core(void *data) {
  esp_mqtt_client_config_t opts = {};

  esp_log_level_set(TAG, ESP_LOG_INFO);

  // wait for network to be ready to ensure dns resolver is available
  Net::waitForReady();

  // build the client id once
  if (_client_id.empty()) {
    _client_id = "ruth-" + Net::macAddress();
  }

  opts.uri = Binder::mqttUri();
  opts.disable_clean_session = false;
  opts.username = Binder::mqttUser();
  opts.password = Binder::mqttPasswd();
  opts.client_id = _client_id.c_str();
  opts.reconnect_timeout_ms = 3000;
  opts.task_prio = 11;
  opts.buffer_size = 1024;
  opts.out_buffer_size = 5120;
  opts.user_context = this;

  _connection = esp_mqtt_client_init(&opts);
  StatusLED::brighter();

  // esp_mqtt_client_register_event(esp_mqtt_client_handle_t client,
  // esp_mqtt_event_id_t event, esp_event_handler_t event_handler, void
  // *event_handler_arg)

  esp_mqtt_client_register_event(_connection,
                                 (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                 eventHandler, _connection);
  esp_mqtt_client_start(_connection);

  // core forever loop
  for (auto announce = true; _run_core;) {
    if (_mqtt_ready && announce) {
      announceStartup();
      announce = false;

      UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(nullptr);
      auto stack_percent =
          100.0 - ((float)stack_high_water / (float)_task.stackSize * 100.0);

      ESP_LOGI(TAG, "after announce stack used[%0.1f%%] hw[%u]", stack_percent,
               stack_high_water);
    }

    if ((_msg_max_size > _msg_max_size_reported) && Net::waitForName(0)) {
      _msg_max_size_reported = _msg_max_size;
      TR::rlog("[MQTT] message size max[%d]", _msg_max_size);
    }

    vTaskDelay(1000);
  }

  // if the core task loop ever exits then a shutdown is underway.

  // a. signal mqtt is no longer ready
  // b. disable auto reconnect
  // c. force disconnection
  // d. destroy (free) the client control structure
  _mqtt_ready = false;
  opts.disable_auto_reconnect = true;
  esp_mqtt_set_config(_connection, &opts);
  esp_mqtt_client_disconnect(_connection);
  esp_mqtt_client_destroy(_connection);
  _connection = nullptr;

  // wait here forever, vTaskDelete will remove us from the scheduler
  vTaskDelay(UINT32_MAX);
}

void MQTT::subACK(esp_mqtt_event_handle_t event) {
  UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(nullptr);

  if (event->msg_id == _subscribe_msg_id) {
    auto stack_size = 6 * 1024;
    auto stack_percent =
        100.0 - ((float)stack_high_water / (float)stack_size * 100.0);

    ESP_LOGI(ESP_TAG, "stack[%0.1f%%] SUBSCRIBED msg_id[%u]", stack_percent,
             event->msg_id);

    _mqtt_ready = true;
    Net::setTransportReady();
    // NOTE: do not announce startup here.  doing so creates a race condition
    // that results in occasionally using epoch as the startup time
  } else {
    ESP_LOGW(TAG, "subACK for UNKNOWN msg_id[%d]", event->msg_id);
  }
}

void MQTT::subscribeFeeds(esp_mqtt_client_handle_t client) {
  const char *topic = _feed_host.c_str();
  int qos = 1; // hardcoded QoS

  _subscribe_msg_id = esp_mqtt_client_subscribe(client, topic, qos);

  ESP_LOGI(TAG, "subscribe feed[%s] msg_id[%d]", topic, _subscribe_msg_id);
}

// STATIC
void MQTT::eventHandler(void *handler_args, esp_event_base_t base,
                        int32_t event_id, void *event_data) {

  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

  eventCallback(event);
}

esp_err_t MQTT::eventCallback(esp_mqtt_event_handle_t event) {
  esp_err_t rc = ESP_OK;
  esp_mqtt_client_handle_t client = event->client;
  esp_mqtt_connect_return_code_t status;

  MQTT_t *mqtt = (MQTT_t *)event->user_context;

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
    mqtt->subscribeFeeds(client);

    break;

  case MQTT_EVENT_DISCONNECTED:
    StatusLED::dim();

    mqtt->connectionClosed();
    break;

  case MQTT_EVENT_SUBSCRIBED:
    mqtt->subACK(event);
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

void MQTT::coreTask(void *task_instance) {
  MQTT_t *mqtt = __singleton__;
  Task_t *task = &(__singleton__->_task);

  mqtt->core(task->data);

  // if the core task ever returns then wait forever
  vTaskDelay(UINT32_MAX);
}

//
// STATIC
//
// NOTE:  MQTT::shutdown() is executed within the CALLING task
//        care must be taken to avoid race conditions
//
void MQTT::shutdown() {
  // make a copy of the instance pointer to delete
  MQTT_t *mqtt = __singleton__;

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

  // ok, the MQTT task has cleanly closed it's connections

  // vTaskDelete removes the task from the scheduler.
  // so we must call mg_mgr_free() before

  // a. signal FreeRTOS to remove the task from the scheduler
  // b. the IDLE task will ultimately free the FreeRTOS memory for the task.
  // the memory allocated by the MQTT instance must be freed in it's
  // destructor.
  vTaskDelete(handle);

  // now that FreeRTOS will no longer schedule the MQTT task set singleton to
  // nullptr and delete the MQTT object

  __singleton__ = nullptr;
  delete mqtt;
}

void MQTT::start() {
  MQTT_t *mqtt = nullptr;
  Task_t *task = nullptr;

  if (__singleton__ == nullptr) {
    __singleton__ = new MQTT();
    _instance_ = __singleton__;
    mqtt = __singleton__;
    task = &(mqtt->_task);
  }

  if (mqtt->_task.handle != nullptr) {
    return;
  }

  // this (object) is passed as the data to the task creation and is
  // used by the static runEngine method to call the run method
  ::xTaskCreate(&coreTask, TAG, task->stackSize, __singleton__, task->priority,
                &(task->handle));
}

//
// Static Class Wrappers for calling public API
//
// the wrapper prevents calls to the instance before it is created

void MQTT::publish(Reading_t *reading) {
  // safety first
  if (_instance_) {
    _instance_->publishMsg(reading->json());
  };
}

void MQTT::publish(Reading_t &reading) {
  if (_instance_) {
    _instance_->publishMsg(reading.json());
  }
}

void MQTT::publish(unique_ptr<Reading_t> reading) {
  if (_instance_) {
    _instance_->publishMsg(reading->json());
  };
}

TaskHandle_t MQTT::taskHandle() {
  return (__singleton__) ? __singleton__->_task.handle : nullptr;
}

} // namespace ruth
