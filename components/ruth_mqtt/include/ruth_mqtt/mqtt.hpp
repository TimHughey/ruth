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

#pragma once

#include "filter/subscribe.hpp"
#include "message/handler.hpp"
#include "message/out.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ruth {

class MQTT {
public:
  struct ConnOpts {
    const char *client_id;
    const char *uri;
    const char *user;
    const char *passwd;
    TaskHandle_t notify_task;
  };

public:
  enum Notifies : uint32_t {
    CONNECTED = 0x01 << 30,
    DISCONNECTED = 0x01 << 29,
    READY = 0x01 << 28,
    QUEUED_MSG = 0x01 << 27
  };

public:
  MQTT() = default;
  ~MQTT() = default; // connection clean-up handled by shutdown

  MQTT(const MQTT &) = delete;
  void operator=(const MQTT &) = delete;

  void connectionClosed();

  void incomingMsg(message::InWrapped msg);
  static void initAndStart(const ConnOpts &opts);
  const ConnOpts &opts() const { return _opts; }

  static void registerHandler(message::Handler *handler);
  static bool send(message::Out &msg);

  // static void shutdown();
  static void subscribe(const filter::Subscribe &filter);
  void subscribeAck(int msg_id);

  static TaskHandle_t taskHandle();

private:
  // static esp_err_t eventCallback(esp_mqtt_event_handle_t event);
  // static void eventHandler(void *args, esp_event_base_t base, int32_t id, void *data);

  // void subscribeAck(esp_mqtt_event_handle_t event);

private:
  ConnOpts _opts;
  bool _mqtt_ready = false;
  int _subscribe_msg_id = 0;

  // esp_mqtt_client_handle_t _connection = nullptr;
  // esp_mqtt_connect_return_code_t _last_return_code;

  static constexpr uint32_t _max_handlers = 10;
  message::Handler *_handlers[_max_handlers];

private:
};
} // namespace ruth
