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
#include <string_view>

#include <esp_event.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <sdkconfig.h>

#include "filter.hpp"
#include "handler.hpp"
#include "out.hpp"
#include "ruth_task.hpp"

namespace ruth {

class MQTT {
public:
  struct Opts {
    const char *client_id;
    const char *uri;
    const char *user;
    const char *passwd;
  };

public:
  MQTT(const Opts &opts);
  ~MQTT();

  MQTT(const MQTT &) = delete;
  void operator=(const MQTT &) = delete;

  static void registerHandler(message::Handler *handler, std::string_view category);
  static bool send(message::Out &msg);
  static void shutdown();
  static void start(const Opts &opts);
  static TaskHandle_t taskHandle();

private:
  void announceStartup();
  void brokerAck() { _broker_acks++; }

  static esp_err_t eventCallback(esp_mqtt_event_handle_t event);
  static void eventHandler(void *args, esp_event_base_t base, int32_t id, void *data);
  void incomingMsg(esp_mqtt_event_t *event);
  void subscribe(esp_mqtt_client_handle_t client);
  void subscribeAck(esp_mqtt_event_handle_t event);

  // void (*esp_event_handler_t)(void *event_handler_arg, esp_event_base_t
  // event_base, int32_t event_id, void *event_data)

  // instance member functions

  void connectionClosed();

  void core(void *data); // actual function that becomes the task main loop
  static void coreTask(void *task_instance);

private:
  typedef struct {
    message::Handler *handler = nullptr;
    char category[24] = {};

    bool matchCategory(const char *to_match) const {
      if (strncmp(category, to_match, sizeof(category)) == 0)
        return true;
      else
        return false;
    }
  } RegisteredHandler;

private:
  // Opts _opts = {};
  message::Filter _rpt_filter;
  uint32_t _rpt_qos = 0;
  message::Filter _host_filter;

  bool _run_core = true;
  // the Ruth MQTT task has the singular responsibility of announcing the startup of this host
  Task_t _task = {.handle = nullptr, .data = nullptr, .priority = 1, .stackSize = 4096};

  esp_mqtt_client_handle_t _connection = nullptr;
  uint64_t _broker_acks = 0;
  bool _mqtt_ready = false;
  esp_mqtt_connect_return_code_t _last_return_code;

  uint16_t _subscribe_msg_id;

  static constexpr uint32_t _max_handlers = 10;
  RegisteredHandler _handlers[_max_handlers];

private:
};
} // namespace ruth

#endif // mqtt_h
