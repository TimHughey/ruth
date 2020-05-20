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
#include "misc/local_types.hpp"
#include "misc/nvs.hpp"
#include "misc/profile.hpp"
#include "misc/restart.hpp"
#include "misc/status_led.hpp"
#include "net/network.hpp"
#include "protocols/mqtt.hpp"
#include "readings/readings.hpp"

namespace ruth {

using std::unique_ptr;
using std::vector;

static MQTT *__singleton__ = nullptr;

// SINGLETON!  use _instance_() for object access
MQTT_t *MQTT::_instance_() {
  if (__singleton__ == nullptr) {
    __singleton__ = new MQTT();
  }

  return __singleton__;
}

// SINGLETON! constructor is private
MQTT::MQTT() {
  // create the report and command feed topics
  _feed_rpt = _feed_prefix + _feed_rpt_prefix + Net::hostID();
  _feed_cmd = _feed_prefix + _feed_cmd_suffix; // DEPRECATED
  _feed_host = _feed_prefix + Net::hostID() + _feed_host_suffix;

  ESP_LOGV(tagEngine(), "reporting to feed=\"%s\"", _feed_rpt.c_str());

  // create the endpoint URI
  const auto max_endpoint = 127;
  unique_ptr<char[]> endpoint(new char[max_endpoint + 1]);
  snprintf(endpoint.get(), max_endpoint, "%s:%d", _host.c_str(), _port);
  _endpoint = endpoint.get();

  _q_out = xQueueCreate(_q_out_len, sizeof(mqttOutMsg_t *));

  ESP_LOGI(tagEngine(), "OUT queue len(%d) msg_size(%u) total_size(%u)",
           _q_out_len, sizeof(mqttOutMsg_t),
           (sizeof(mqttOutMsg_t) * _q_out_len));
}

void MQTT::announceStartup() {
  uint32_t batt_mv = Core::batteryMilliVolt();
  startupReading_t startup(batt_mv);

  _publish(&startup);
  StatusLED::off();
}

void MQTT::connect(int wait_ms) {
  // establish a unique client id
  _client_id = "ruth-" + Net::macAddress();

  if (wait_ms > 0) {
    TickType_t last_wake = xTaskGetTickCount();
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(wait_ms));
  }

  Net::waitForReady();

  StatusLED::brighter();
  _connection = mg_connect(&_mgr, _endpoint.c_str(), _ev_handler);

  if (_connection) {
    ESP_LOGD(tagEngine(), "created pending mongoose connection(%p) for %s",
             _connection, _endpoint.c_str());
  }
}

void MQTT::connectionClosed() {
  StatusLED::dim();
  ESP_LOGW(tagEngine(), "connection closed");
  _mqtt_ready = false;
  _connection = nullptr;

  Net::clearTransportReady();

  connect();
}

bool MQTT::handlePayload(MsgPayload_t *payload) {
  auto processed = false;

  if (payload->matchSubtopic("pwm")) {
    auto rc = PulseWidth::queuePayload(payload);

    if (rc == false) {
      ESP_LOGW(tagEngine(), "PulseWidth::queueCommand() FAILED");
    }

    processed = true;
  }

  if (payload->matchSubtopic("i2c")) {
    auto rc = I2c::queuePayload(payload);

    if (rc == false) {
      ESP_LOGW(tagEngine(), "I2c::queueCommand() FAILED");
    }

    processed = true;
  }

  if (payload->matchSubtopic("ds")) {
    auto rc = DallasSemi::queuePayload(payload);

    if (rc == false) {
      ESP_LOGW(tagEngine(), "DallasSemi::queueCommand() FAILED");
    }

    processed = true;
  }

  if (payload->matchSubtopic("profile")) {
    unique_ptr<MsgPayload_t> payload_ptr(payload);
    if (Profile::parseRawMsg(payload->payload())) {
      Profile::postParseActions();
    }

    processed = true;
  }

  if (payload->matchSubtopic("ota")) {
    OTA *ota = OTA::payload(payload);
    Core::otaRequest(ota);

    processed = true;
  }

  if (payload->matchSubtopic("restart")) {
    Core::restartRequest();

    processed = true;
  }

  return processed;
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

  MsgPayload_t *payload = new MsgPayload(in_topic, in_payload);

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

  // NOTE:  the downstream object and/or task must free payload when finished
  if (handlePayload(payload)) {
    return;
  } else {
    textReading_t *rlog = new textReading();
    textReading_ptr_t rlog_ptr(rlog);

    rlog->printf("[MQTT] unable to process subtopic: \"%s\"",
                 payload->subtopic().c_str());
    rlog->publish();

    delete payload;
  }
}

// the message did not have a subtopic, send it to MQTTin for handling

// queue send takes a pointer to what should be copied to the queue
// using the size defined when the queue was created
// BaseType_t q_rc;
// q_rc = xQueueSendToBack(_q_in, (void *)&payload, _inbound_rb_wait_ticks);
//
// if (q_rc) {
//   ESP_LOGV(tagEngine(), "INCOMING payload SENT to QUEUE
//   (len=%u,msg_len=%u)",
//            sizeof(MsgPayload_t), in_payload->len);
// } else {
//   delete payload;
//
//   char *msg = (char *)calloc(sizeof(char), 128);
//   sprintf(msg, "RECEIVE msg FAILED (len=%u)", in_payload->len);
//   ESP_LOGW(tagEngine(), "%s", msg);
//
//   // we only commit the failure to NVS and directly call esp_restart()
//   // since MQTT is broken
//   NVS::commitMsg(tagEngine(), msg);
//   free(msg);
//
//   // pass a nullptr for the message so Restart doesn't attempt to publish
//   Restart::restart(nullptr, __PRETTY_FUNCTION__);

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
  size_t len = 0;
  mqttOutMsg_t *entry;
  auto q_rc = pdFALSE;

  q_rc = xQueueReceive(_q_out, &entry, _outbound_msg_ticks);

  while ((q_rc == pdTRUE) && Net::waitForReady(0)) {
    elapsedMicros publish_elapse;

    const auto *msg = entry->data;
    size_t msg_len = entry->len;

    ESP_LOGV(tagEngine(), "send msg(len=%u), payload(len=%u)", len, msg_len);

    mg_mqtt_publish(_connection, _feed_rpt.c_str(), _msg_id++, MG_MQTT_QOS(1),
                    msg->data(), msg_len);

    delete msg;
    delete entry;

    int64_t publish_us = publish_elapse;
    if (publish_us > 3000) {
      ESP_LOGD(tagOutbound(), "publish msg took %0.2fms",
               ((float)publish_us / 1000.0));
    } else {
      ESP_LOGV(tagOutbound(), "publish msg took %lluus", publish_us);
    }

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

    ESP_LOGW(tagEngine(), "%s", log_msg.get());

    // we only commit the failure to NVS and directly call esp_restart()
    // since MQTT is broken
    NVS::commitMsg(tagEngine(), log_msg.get());

    // pass a nullptr for the message so Restart doesn't attempt to publish
    Restart::restart(nullptr, __PRETTY_FUNCTION__);
  }
}

void MQTT::core(void *data) {
  struct mg_mgr_init_opts opts = {};

  esp_log_level_set(tagEngine(), ESP_LOG_INFO);

  // wait for network to be ready to ensure dns resolver is available
  ESP_LOGI(tagEngine(), "waiting for network...");
  Net::waitForReady();

  // mongoose uses it's own dns resolver so set the namserver from dhcp
  opts.nameserver = Net::dnsIP();

  mg_mgr_init_opt(&_mgr, NULL, opts);

  connect();

  bool startup_announced = false;
  elapsedMillis announce_startup_delay;

  for (;;) {
    // send the startup announcement once the time is available.
    // this solves a race condition when mqtt connection and subscription
    // to the command feed completes before the time is set and avoids
    // sending a startup announcement with epoch as the timestamp
    if ((startup_announced == false) && (Net::isTimeSet()) &&
        (announce_startup_delay > 300)) {
      ESP_LOGV(tagEngine(), "announcing startup");
      announceStartup();

      startup_announced = true;
    }

    // to alternate between prioritizing send and recv:
    //  1. wait here (recv)
    //  2. wait in outboundMsg (send)
    mg_mgr_poll(&_mgr, _inbound_msg_ms);

    if (isReady() && _connection) {
      outboundMsg();
    }
  }
}

void MQTT::_subACK_(struct mg_mqtt_message *msg) {

  if (msg->message_id == _subscribe_msg_id) {
    ESP_LOGV(tagEngine(), "subACK for EXPECTED msg_id=%d", msg->message_id);
    _mqtt_ready = true;
    Net::setTransportReady();
    // NOTE: do not announce startup here.  doing so creates a race condition
    // that results in occasionally using epoch as the startup time
  } else {
    ESP_LOGW(tagEngine(), "subACK for UNKNOWN msg_id=%d", msg->message_id);
  }
}

void MQTT::_subscribeFeeds_(struct mg_connection *nc) {

  struct mg_mqtt_topic_expression sub[] = {
      {.topic = _feed_host.c_str(), .qos = 1}};

  _subscribe_msg_id = _msg_id++;
  ESP_LOGI(tagEngine(), "subscribe feeds \"%s\" msg_id=%d", sub[0].topic,
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
    ESP_LOGV(MQTT::tagEngine(), "CONNECT msg=%p err_code=%d err_str=%s",
             (void *)msg, *status, strerror(*status));

    MQTT::handshake(nc);
    StatusLED::off();
    break;
  }

  case MG_EV_MQTT_CONNACK:
    if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
      ESP_LOGW(MQTT::tagEngine(), "mqtt connection error: %d",
               msg->connack_ret_code);
      return;
    }

    ESP_LOGV(MQTT::tagEngine(), "MG_EV_MQTT_CONNACK rc=%d",
             msg->connack_ret_code);
    MQTT::subscribeFeeds(nc);

    break;

  case MG_EV_MQTT_SUBACK:
    MQTT::subACK(msg);

    break;

  case MG_EV_MQTT_SUBSCRIBE:
    ESP_LOGV(MQTT::tagEngine(), "subscribe event, payload=%s", msg->payload.p);
    break;

  case MG_EV_MQTT_UNSUBACK:
    ESP_LOGV(MQTT::tagEngine(), "unsub ack");
    break;

  case MG_EV_MQTT_PUBLISH:
    // topic.assign(msg->topic.p, msg->topic.len);
    // ESP_LOGI(MQTT::tagEngine(), "%s qos(%d)", topic.c_str(), msg->qos);
    if (msg->qos == 1) {

      mg_mqtt_puback(MQTT::_instance_()->_connection, msg->message_id);
    }

    MQTT::incomingMsg(&(msg->topic), &(msg->payload));
    break;

  case MG_EV_MQTT_PINGRESP:
    ESP_LOGV(MQTT::tagEngine(), "ping response");
    break;

  case MG_EV_CLOSE:
    StatusLED::dim();
    MQTT::_instance_()->connectionClosed();
    break;

  case MG_EV_POLL:
  case MG_EV_RECV:
  case MG_EV_SEND:
  case MG_EV_MQTT_PUBACK:
    // events to ignore
    break;

  default:
    ESP_LOGW(MQTT::tagEngine(), "unhandled event 0x%04x", ev);
    break;
  }
}
} // namespace ruth
