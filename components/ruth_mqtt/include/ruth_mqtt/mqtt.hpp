//  Ruth
//  Copyright (C) 2017  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "message/handler.hpp"
#include "message/out.hpp"
#include "ru_base/types.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <memory>
#include <mqtt_client.h>
#include <optional>

namespace ruth {

class MQTT;

namespace shared {
extern std::optional<MQTT> mqtt;
}

class MQTT {
public:
  struct ConnOpts {
    ConnOpts(ccs client_id, ccs uri, ccs user, ccs passwd) noexcept
        : client_id(client_id),                    //
          uri(uri),                                //
          user(user),                              //
          passwd(passwd),                          //
          notify_task(xTaskGetCurrentTaskHandle()) //
    {}
    ~ConnOpts() = default;

    const char *client_id;
    const char *uri;
    const char *user;
    const char *passwd;
    TaskHandle_t notify_task;
  };

private:
  enum Notifies : uint32_t {
    CONNECTED = 0x01 << 30,
    DISCONNECTED = 0x01 << 29,
    READY = 0x01 << 28,
    QUEUED_MSG = 0x01 << 27
  };

public:
  MQTT(ConnOpts &&opts, message::Handler *handler) noexcept;
  ~MQTT() = default; // connection clean-up handled by shutdown

  static void event_handler(void *handler_args, esp_event_base_t base, int32_t event_id,
                            void *event_data) noexcept;

  static bool hold_for_connection(int32_t max_wait_ms = 60000) noexcept;

  void incomingMsg(message::InWrapped msg) noexcept;

  static void registerHandler(message::Handler *handler) noexcept;
  static bool send(message::Out &&msg) noexcept;

private:
  void subscribe() noexcept;
  void subscribe_ack(const char *tag, int msg_id) noexcept;

private:
  // order dependent
  const ConnOpts opts;
  esp_mqtt_client_handle_t conn;
  bool mqtt_ready;
  int sub_msg_id;
  uint64_t broker_acks;
  std::array<message::Handler *, 10> handlers;

private:
  TaskHandle_t self{nullptr};
  esp_err_t client_start_rc{0};
};
} // namespace ruth
