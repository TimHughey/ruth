/*
    mqtt.h - Ruth MQTT
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

#ifndef ruth_mqtt_h
#define ruth_mqtt_h

#include <cstdlib>
#include <memory>
#include <string>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <sdkconfig.h>

#include "external/mongoose.h"
#include "protocols/mqtt_in.hpp"
#include "readings/readings.hpp"

namespace ruth {

typedef struct {
  size_t len = 0;
  string_t *data = nullptr;
} mqttOutMsg_t;

typedef class MQTT MQTT_t;
class MQTT {
public:
  static void start() { _instance_()->_start_(); }
  static void publish(Reading_t *reading) { _instance_()->_publish_(reading); }
  static void publish(Reading_t &reading) { _instance_()->_publish_(reading); }
  static void publish(unique_ptr<Reading_t> reading) {
    _instance_()->_publish_(move(reading));
  }

  void connect(int wait_ms = 0);
  void connectionClosed();

  static void handshake(struct mg_connection *nc) {
    _instance_()->_handshake_(nc);
  }
  static void subscribeFeeds(struct mg_connection *nc) {
    _instance_()->_subscribeFeeds_(nc);
  }

  static void subACK(struct mg_mqtt_message *msg) {
    _instance_()->_subACK_(msg);
  }
  static void incomingMsg(struct mg_str *topic, struct mg_str *payload) {
    _instance_()->_incomingMsg_(topic, payload);
  };
  bool isReady() { return _mqtt_ready; };

  void core(void *data);

  void stop() {
    if (_task.handle == nullptr) {
      return;
    }

    xTaskHandle temp = _task.handle;
    _task.handle = nullptr;
    ::vTaskDelete(temp);
  }

  static const char *tagEngine() { return "MQTT"; };
  static const char *tagOutbound() { return "MQTT outboundMsg"; };
  TaskHandle_t taskHandle() { return _task.handle; };

private:
  MQTT(); // singleton, constructor is private
  static MQTT_t *_instance_();
  void _handshake_(struct mg_connection *nc);
  void _incomingMsg_(struct mg_str *topic, struct mg_str *payload);
  void _subscribeFeeds_(struct mg_connection *nc);
  void _subACK_(struct mg_mqtt_message *msg);

  void _start_(void *task_data = nullptr) {
    if (_task.handle != nullptr) {
      ESP_LOGW(tagEngine(), "there may already be a task running %p",
               (void *)_task.handle);
    }

    // this (object) is passed as the data to the task creation and is
    // used by the static runEngine method to call the run method
    ::xTaskCreate(&runEngine, tagEngine(), _task.stackSize, this,
                  _task.priority, &_task.handle);
  }

  static void _ev_handler(struct mg_connection *nc, int ev, void *p);

  string_t _client_id = "ruth.xxxxxxxxxxxx +extra";
  string_t _endpoint = "example.wisslanding.com +extra";
  Task_t _task = {.handle = nullptr,
                  .data = nullptr,
                  .lastWake = 0,
                  .priority = 14,
                  .stackSize = (4 * 1024)};

  struct mg_mgr _mgr = {};
  struct mg_connection *_connection = nullptr;
  uint16_t _msg_id = (uint16_t)esp_random() + 1;
  bool _mqtt_ready = false;

  // mg_mgr uses LWIP and the timeout is specified in ms
  int _inbound_msg_ms = 1;
  TickType_t _inbound_rb_wait_ticks = pdMS_TO_TICKS(1000);
  TickType_t _outbound_msg_ticks = pdMS_TO_TICKS(30);

  const size_t _q_out_len = (sizeof(mqttOutMsg_t) * 128);
  const size_t _q_in_len = ((sizeof(mqttInMsg_t) * 128) / 2);
  QueueHandle_t _q_out = nullptr;
  QueueHandle_t _q_in = nullptr;

  MQTTin_t *_mqtt_in = nullptr;

  // const char *_dns_server = CONFIG_RUTH_DNS_SERVER;
  const string_t _host = CONFIG_RUTH_MQTT_HOST;
  const int _port = CONFIG_RUTH_MQTT_PORT;
  const char *_user = CONFIG_RUTH_MQTT_USER;
  const char *_passwd = CONFIG_RUTH_MQTT_PASSWD;

  // NOTES:
  //   1. final feeds are built in the constructor
  //   2. feeds are always prefixed by the environment
  //   3. the '+extra' is intended to preallocate enough space to prevent
  //      string_t re-allocs while building the actual feed
  const string_t _feed_prefix = CONFIG_RUTH_ENV "/";
  string_t _feed_rpt_config = "ruth/f/report";
  string_t _feed_cmd_config = "ruth/f/command";
  string_t _feed_rpt_actual = CONFIG_RUTH_ENV "/ruth/f/command +extra";
  string_t _feed_cmd_actual = CONFIG_RUTH_ENV "/ruth/f/report +extra";
  string_t _feed_host_actual = CONFIG_RUTH_ENV "/ruth.xxxxxxxxxxxx/# +extra";

  uint16_t _subscribe_msg_id;

  void announceStartup();
  void outboundMsg();

  void _publish_(Reading_t *reading);
  void _publish_(Reading_t &reading);
  void _publish_(std::unique_ptr<Reading_t> reading);

  void publish_msg(string_t *msg);

  // Task implementation
  static void runEngine(void *task_instance) {
    MQTT_t *task = (MQTT_t *)task_instance;
    task->core(task->_task.data);
  }
};
} // namespace ruth

#endif // mqtt_h
