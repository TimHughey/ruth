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

#ifndef _ruth_mqtt_hpp
#define _ruth_mqtt_hpp

#include <cstdlib>
#include <memory>
#include <string>

#include <esp_event.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <sdkconfig.h>

#include "protocols/payload.hpp"
#include "readings/readings.hpp"

namespace ruth {

using namespace reading;

typedef struct {
  size_t len = 0;
  string_t *data = nullptr;
} mqttOutMsg_t;

typedef class MQTT MQTT_t;
class MQTT {
public:
  // allocate the singleton and start MQTT task
  static void start();

  ~MQTT();

  // publish messages to the reporting feed
  // these functions can be called safety even if MQTT is not started
  // or has been shutdown
  static void publish(Reading_t *reading);
  static void publish(Reading_t &reading);
  static void publish(unique_ptr<Reading_t> reading);

  // close MQTT connections and delete the task
  static void shutdown();

  static TaskHandle_t taskHandle();

private:
  MQTT(); // singleton, constructor is private

  // member functions via static singleton to handle events
  // and expose public API (e.g. publish)
  void _brokerAck_() { _broker_acks++; }
  void _incomingMsg_(esp_mqtt_event_t *event);
  void _publish(Reading_t *reading);
  void _publish(Reading_t &reading);
  void _publish(Reading_ptr_t reading);
  void _subscribeFeeds_(esp_mqtt_client_handle_t client);
  void _subACK_(esp_mqtt_event_handle_t event);

  // void (*esp_event_handler_t)(void *event_handler_arg, esp_event_base_t
  // event_base, int32_t event_id, void *event_data)
  static void _ev_handler(void *handler_args, esp_event_base_t base,
                          int32_t event_id, void *event_data);

  static esp_err_t _ev_callback(esp_mqtt_event_handle_t event);

private:
  // private member variables
  esp_mqtt_client_config_t mqtt_cfg;

  // private strings defining essential connection info
  // const char *_dns_server = CONFIG_RUTH_DNS_SERVER;
  // const string_t _host = CONFIG_RUTH_MQTT_HOST;
  const string_t _uri = CONFIG_RUTH_MQTT_URI;
  const char *_user = CONFIG_RUTH_MQTT_USER;
  const char *_passwd = CONFIG_RUTH_MQTT_PASSWD;

  string_t _client_id;

  // NOTES:
  //   1. final feeds are built in the constructor
  //   2. feeds are always prefixed by the environment
  //   3. should include the actual host ID
  //   4. end with the corresponding suffix
  const string_t _feed_prefix = CONFIG_RUTH_ENV "/";

  // NOTE:  _feed_host is replacing _feed_cmd
  string_t _feed_host_suffix = "/#";
  string_t _feed_rpt_prefix = "r/";
  string_t _feed_host;
  string_t _feed_rpt;

  bool _run_core = true;
  bool _shutdown = false;
  Task_t _task = {.handle = nullptr,
                  .data = nullptr,
                  .lastWake = 0,
                  .priority = 13,
                  .stackSize = (5 * 1024)};

  esp_mqtt_client_handle_t _connection = nullptr;
  uint16_t _msg_id = (uint16_t)esp_random() + 1;
  uint64_t _broker_acks = 0;
  bool _mqtt_ready = false;

  // prioritize inbound messages
  TickType_t _outbound_msg_ticks = pdMS_TO_TICKS(30);

  const size_t _q_out_len = 64;
  QueueHandle_t _q_out = nullptr;

  uint16_t _subscribe_msg_id;

private:
  // instance member functions
  void announceStartup();

  // actual function that becomes the task main loop
  void core(void *data);
  bool handlePayload(MsgPayload_t_ptr payload);
  void outboundMsg();

  void publish_msg(string_t *msg);
  void connectionClosed();

  // Task implementation
  static void _start_(void *task_data = nullptr);
  static void coreTask(void *task_instance);
};
} // namespace ruth

#endif // mqtt_h
