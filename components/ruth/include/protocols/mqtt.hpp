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

#include <esp_event.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <sdkconfig.h>

#include "core/binder.hpp"
#include "protocols/payload.hpp"
#include "readings/readings.hpp"

namespace ruth {

using namespace reading;

typedef class MQTT MQTT_t;
typedef class TextBuffer<24> MqttClientId_t;
typedef class TextBuffer<4> FeedPrefix_t;
typedef class TextBuffer<30> Feed_t;

class MQTT {
public:
  MQTT() = default; // singleton
  static void start();

  ~MQTT();

  // publish messages to the reporting feed
  // these functions can be called safety even if MQTT is not started
  // or has been shutdown
  static void publish(Reading_t *reading);
  static void publish(Reading_t &reading);
  static void publish(const WatcherPayload_t &payload);
  // static void publish(unique_ptr<Reading_t> reading);

  // close MQTT connections and delete the task
  static void shutdown();

  static TaskHandle_t taskHandle();

private:
  // member functions via static singleton to handle events
  // and expose public API (e.g. publish)
  void brokerAck() { _broker_acks++; }
  void incomingMsg(esp_mqtt_event_t *event);
  void subscribeFeeds(esp_mqtt_client_handle_t client);
  void subACK(esp_mqtt_event_handle_t event);

  // void (*esp_event_handler_t)(void *event_handler_arg, esp_event_base_t
  // event_base, int32_t event_id, void *event_data)
  static void eventHandler(void *handler_args, esp_event_base_t base,
                           int32_t event_id, void *event_data);

  static esp_err_t eventCallback(esp_mqtt_event_handle_t event);

private:
  // private member variables
  esp_mqtt_client_config_t mqtt_cfg;
  MqttClientId_t _client_id;

  // NOTES:
  //   1. final feeds are built in start()
  //   2. feeds are always prefixed by the environment
  //   3. should include the actual host ID
  //   4. end with the corresponding suffix
  FeedPrefix_t _feed_prefix;

  Feed_t _feed_rpt;
  uint32_t _feed_qos = 0;
  Feed_t _feed_host;

  bool _run_core = true;
  Task_t _task = {
      .handle = nullptr, .data = nullptr, .priority = 1, .stackSize = 4096};

  esp_mqtt_client_handle_t _connection = nullptr;
  int32_t _msg_id = esp_random() + 1;
  uint64_t _broker_acks = 0;
  bool _mqtt_ready = false;
  esp_mqtt_connect_return_code_t _last_return_code;

  uint16_t _subscribe_msg_id;

private:
  // instance member functions
  void announceStartup();

  // actual function that becomes the task main loop
  void core(void *data);
  bool handlePayload(MsgPayload_t_ptr payload);

  bool publishActual(const char *msg, size_t len);

  inline void publishMsg(const MsgPackPayload_t &payload) {
    publishActual(payload.c_str(), payload.size());
  }

  inline void publishMsg(const WatcherPayload_t &payload) {
    publishActual(payload.c_str(), payload.size());
  }

  void connectionClosed();

  // Task implementation
  static void _start_(void *task_data = nullptr);
  static void coreTask(void *task_instance);
};
} // namespace ruth

#endif // mqtt_h
