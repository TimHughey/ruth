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
#include "external/mongoose.h"
#include "local/types.hpp"
#include "misc/nvs.hpp"
#include "misc/restart.hpp"
#include "misc/status_led.hpp"
#include "net/network.hpp"
#include "net/profile/profile.hpp"
#include "protocols/mqtt.hpp"
#include "readings/readings.hpp"

namespace ruth {

using std::move;
using std::unique_ptr;
using std::vector;

using TR = ruth::textReading_t;

static const char *TAG = "MQTT";
// __singleton__ is used by private MQTT static functions
static MQTT *__singleton__ = nullptr;
// _instance_ is used for public API
static MQTT *_instance_ = nullptr;

// SINGLETON! constructor is private
MQTT::MQTT() {
  // create the report and command feed topics
  _feed_rpt = _feed_prefix + _feed_rpt_prefix + Net::hostID();
  _feed_host = _feed_prefix + Net::hostID() + _feed_host_suffix;

  ESP_LOGV(TAG, "reporting to feed=\"%s\"", _feed_rpt.c_str());

  // create the endpoint URI
  const auto max_endpoint = 127;
  unique_ptr<char[]> endpoint(new char[max_endpoint + 1]);
  snprintf(endpoint.get(), max_endpoint, "%s:%d", _host.c_str(), _port);
  _endpoint = endpoint.get();

  _q_out = xQueueCreate(_q_out_len, sizeof(mqttOutMsg_t *));

  ESP_LOGI(TAG, "OUT queue len(%d) msg_size(%u) total_size(%u)", _q_out_len,
           sizeof(mqttOutMsg_t), (sizeof(mqttOutMsg_t) * _q_out_len));
}

MQTT::~MQTT() {
  // direct allocations outside of member variables are:
  //  a. the outbound msg queue
  //  b. mg_mgr
  //
  // the mg_mgr is freed by shutdown()

  // so, the only thing to free are the items in the queue (if any) and the
  // queue itself.

  // pop each message from the queue with zero wait ticks until
  // xQueueReceive() returns something other than pdTRUE

  // mg_mgr_free() is called before the destructor ensuring that this loop
  // is only freeing any messages (most likely zero) that hadn't been published
  mqttOutMsg_t *msg;
  while (xQueueReceive(_q_out, &msg, 0) == pdTRUE) {
    // the msg is a simple struct so free the enclosed data

    // TODO: refactor mqttOutMsg_t so this isn't necessary
    delete msg->data;
    delete msg;
  }

  vQueueDelete(_q_out);
}

void MQTT::announceStartup() {
  uint32_t batt_mv = Core::batteryMilliVolt();
  startupReading_t startup(batt_mv);

  _publish(&startup);
  StatusLED::off();
}

void MQTT::connect() {

  // if a shutdown is underway then do not attempt to connect
  if (_shutdown) {
    return;
  }

  // build the client id once
  if (_client_id.empty()) {
    _client_id = "ruth-" + Net::macAddress();
  }

  Net::waitForReady();

  StatusLED::brighter();
  _connection = mg_connect(&_mgr, _endpoint.c_str(), _ev_handler);
}

void MQTT::connectionClosed() {
  StatusLED::dim();

  _mqtt_ready = false;
  _connection = nullptr;

  Net::clearTransportReady();

  // always attempt to reconnect
  connect();
}

bool MQTT::handlePayload(MsgPayload_t_ptr payload_ptr) {
  auto payload_rc = false;
  auto payload = payload_ptr.get();

  if (payload->invalid()) {
    TR::rlog("[MQTT] invalid topic=\"%s\"", payload->errorTopic());
  }

  if (payload->matchSubtopic("pwm")) {
    // move along the payload_ptr
    payload_rc = PulseWidth::queuePayload(move(payload_ptr));

  } else if (payload->matchSubtopic("i2c")) {
    payload_rc = I2c::queuePayload(move(payload_ptr));

  } else if (payload->matchSubtopic("ds")) {
    payload_rc = DallasSemi::queuePayload(move(payload_ptr));

  } else if (payload->matchSubtopic("profile")) {
    Profile::fromRaw(payload);
    payload_rc = Profile::valid();

    if (payload_rc) {
      Profile::postParseActions();
    }
  } else if (payload->matchSubtopic("ota")) {
    // from MQTT's perspective the payload was successful
    payload_rc = true;

    OTA *ota = OTA::payload(move(payload_ptr));
    Core::otaRequest(ota);

  } else if (payload->matchSubtopic("restart")) {

    // from MQTT's perspective the payload was successful
    payload_rc = true;
    Core::restartRequest();
  }

  if (payload_rc == false) {
    TR::rlog("[MQTT] subtopic=\"%s\" failed", payload->subtopic_c());
  }

  return payload_rc;
}

void MQTT::_handshake_(struct mg_connection *nc) {
  struct mg_send_mqtt_handshake_opts opts;
  bzero(&opts, sizeof(opts));

  opts.flags =
      MG_MQTT_CLEAN_SESSION | MG_MQTT_HAS_PASSWORD | MG_MQTT_HAS_USER_NAME;
  opts.user_name = _user;
  opts.password = _passwd;

  mg_set_protocol_mqtt(nc);
  mg_send_mqtt_handshake_opt(nc, _client_id.c_str(), opts);
}

void MQTT::_incomingMsg_(struct mg_str *in_topic, struct mg_str *in_payload) {

  // ensure there is actually a payload to handle
  if (in_payload->len == 0)
    return;

  // we wrap the newly allocated MsgPayload in a unique_ptr then move
  // it downstream to the various classes that process it.  the only potential
  // memory leak is when the payload is queued to an Engine.
  MsgPayload_t_ptr payload_ptr(new MsgPayload(in_topic, in_payload));

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

void MQTT::_publish(Reading_t *reading) {
  auto *msg = reading->json();

  publish_msg(msg);
}

void MQTT::_publish(Reading_t &reading) {
  auto *msg = reading.json();

  publish_msg(msg);
}

void MQTT::_publish(Reading_ptr_t reading) {
  auto *msg = reading->json();

  publish_msg(msg);
}

void MQTT::outboundMsg() {
  mqttOutMsg_t *entry;
  auto q_rc = pdFALSE;

  q_rc = xQueueReceive(_q_out, &entry, _outbound_msg_ticks);

  while ((q_rc == pdTRUE) && Net::waitForReady(0)) {
    const auto *msg = entry->data;
    size_t msg_len = entry->len;

    mg_mqtt_publish(_connection, _feed_rpt.c_str(), _msg_id++, MG_MQTT_QOS(1),
                    msg->data(), msg_len);

    delete msg;
    delete entry;

    q_rc = xQueueReceive(_q_out, &entry, pdMS_TO_TICKS(20));
  }
}

void MQTT::publish_msg(string_t *msg) {
  auto q_rc = pdFALSE;
  mqttOutMsg_t *entry = new mqttOutMsg_t;

  // setup the entry noting that the actual pointer to the string will
  // be included so be certain to deallocate when it comes out of the
  // ringbuffer
  entry->len = msg->length();
  entry->data = msg;

  // queue send takes a pointer to what should be copied to the queue
  // using the size defined when the queue was created
  q_rc = xQueueSendToBack(_q_out, (void *)&entry, pdMS_TO_TICKS(50));

  if (q_rc == pdFALSE) {
    delete entry;
    delete msg;

    unique_ptr<char[]> log_msg(new char[128]);
    auto space_avail = uxQueueSpacesAvailable(_q_out);

    sprintf(log_msg.get(), "PUBLISH msg FAILED space_avail(%d)", space_avail);

    ESP_LOGW(TAG, "%s", log_msg.get());

    // we only commit the failure to NVS and directly call esp_restart()
    // since MQTT is broken
    NVS::commitMsg(TAG, log_msg.get());

    // pass a nullptr for the message so Restart doesn't attempt to publish
    Restart::restart(nullptr, __PRETTY_FUNCTION__);
  }
}

void MQTT::core(void *data) {
  struct mg_mgr_init_opts opts = {};

  esp_log_level_set(TAG, ESP_LOG_INFO);

  // wait for network to be ready to ensure dns resolver is available
  Net::waitForReady();

  // mongoose uses it's own dns resolver so set the namserver from dhcp
  opts.nameserver = Net::dnsIP();

  mg_mgr_init_opt(&_mgr, NULL, opts);

  connect();

  // core forever loop
  // elapsedMillis main_elapsed;
  for (auto announce = true; _run_core;) {
    // if (announce && ((uint64_t)main_elapsed > 2000)) {
    //   announceStartup();
    //   announce = false;
    // }

    // to alternate between prioritizing send and recv:
    //  1. wait here (recv)
    //  2. wait in outboundMsg (send)
    mg_mgr_poll(&_mgr, _inbound_msg_ms);

    if (_mqtt_ready && _connection) {
      outboundMsg();
    }

    if (announce) {
      announceStartup();
      announce = false;
    }
  }

  // if the core task loop ever exits then a shutdown is underway.

  // a. set _shutdown=true to prevent autoconnect
  // b. free the mg_mgr
  // c. signel mqtt is no longer ready
  _shutdown = true;
  mg_mgr_free(&_mgr);
  _mqtt_ready = false;

  // wait here forever, vTaskDelete will remove us from the scheduler
  vTaskDelay(UINT32_MAX);
}

void MQTT::_subACK_(struct mg_mqtt_message *msg) {

  if (msg->message_id == _subscribe_msg_id) {
    ESP_LOGV(TAG, "subACK for EXPECTED msg_id=%d", msg->message_id);
    _mqtt_ready = true;
    Net::setTransportReady();
    // NOTE: do not announce startup here.  doing so creates a race condition
    // that results in occasionally using epoch as the startup time
  } else {
    ESP_LOGW(TAG, "subACK for UNKNOWN msg_id=%d", msg->message_id);
  }
}

void MQTT::_subscribeFeeds_(struct mg_connection *nc) {

  struct mg_mqtt_topic_expression sub[] = {
      {.topic = _feed_host.c_str(), .qos = 1}};

  _subscribe_msg_id = _msg_id++;
  ESP_LOGI(TAG, "subscribe feeds \"%s\" msg_id=%d", sub[0].topic,
           _subscribe_msg_id);
  mg_mqtt_subscribe(nc, sub, 1, _subscribe_msg_id);
}

// STATIC
void MQTT::_ev_handler(struct mg_connection *nc, int ev, void *p) {
  auto *msg = (struct mg_mqtt_message *)p;
  // string_t topic;

  switch (ev) {
  case MG_EV_CONNECT: {
    int *status = (int *)p;
    ESP_LOGV(TAG, "CONNECT msg=%p err_code=%d err_str=%s", (void *)msg, *status,
             strerror(*status));

    __singleton__->_handshake_(nc);
    StatusLED::off();
    break;
  }

  case MG_EV_MQTT_CONNACK:
    if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
      ESP_LOGW(TAG, "mqtt connection error: %d", msg->connack_ret_code);
      return;
    }

    ESP_LOGV(TAG, "MG_EV_MQTT_CONNACK rc=%d", msg->connack_ret_code);
    __singleton__->_subscribeFeeds_(nc);

    break;

  case MG_EV_MQTT_SUBACK:
    __singleton__->_subACK_(msg);

    break;

  case MG_EV_MQTT_SUBSCRIBE:
    ESP_LOGV(TAG, "subscribe event, payload=%s", msg->payload.p);
    break;

  case MG_EV_MQTT_UNSUBACK:
    ESP_LOGV(TAG, "unsub ack");
    break;

  case MG_EV_MQTT_PUBLISH:
    // topic.assign(msg->topic.p, msg->topic.len);
    // ESP_LOGI(TAG, "%s qos(%d)", topic.c_str(), msg->qos);
    if (msg->qos == 1) {

      mg_mqtt_puback(__singleton__->_connection, msg->message_id);
    }

    __singleton__->_incomingMsg_(&(msg->topic), &(msg->payload));
    break;

  case MG_EV_MQTT_PINGRESP:
    ESP_LOGV(TAG, "ping response");
    break;

  case MG_EV_CLOSE:
    StatusLED::dim();
    __singleton__->connectionClosed();
    break;

  case MG_EV_POLL:
  case MG_EV_RECV:
  case MG_EV_SEND:
  case MG_EV_MQTT_PUBACK:
    // events to ignore
    break;

  default:
    ESP_LOGW(TAG, "unhandled event 0x%04x", ev);
    break;
  }
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
  // the memory allocated by the MQTT instance must be freed in it's destructor.
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
    ESP_LOGW(TAG, "task exists [%p]", (void *)mqtt->_task.handle);
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
    _instance_->_publish(reading);
  };
}

void MQTT::publish(Reading_t &reading) {
  if (_instance_) {
    _instance_->_publish(reading);
  }
}

void MQTT::publish(unique_ptr<Reading_t> reading) {
  if (_instance_) {
    _instance_->_publish(move(reading));
  };
}

TaskHandle_t MQTT::taskHandle() {
  return (__singleton__) ? __singleton__->_task.handle : nullptr;
}

} // namespace ruth
