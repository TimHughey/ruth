//  Ruth
//  Copyright (C) 2022  Tim Hughey
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

#include "dmx/dmx.hpp"
#include "inject/inject.hpp"
#include "io/io.hpp"
#include "misc/elapsed.hpp"
#include "msg.hpp"
#include "ru_base/types.hpp"
#include "ru_base/uint8v.hpp"
#include "session/stats.hpp"

#include "ArduinoJson.h"
#include <esp_log.h>
#include <memory>
#include <optional>

namespace ruth {
namespace desk {

class Session;
namespace active {
extern std::optional<Session> session;
}

class Session {

private:
  // use init() to construct a new Session
  inline Session(const session::Inject &di)        // create the session
      : server_io_ctx(di.io_ctx),                  // server io_ctx (for session cleanup)
        socket_ctrl(std::move(di.socket)),         // move the socket connection accepted
        idle_timer(server_io_ctx),                 // idle timer
        fps_timer(server_io_ctx, Millis(2000)),    // frames per second timer
        endpoint_data(ip_udp::v4(), 0),            // udp data packets endpoint
        socket_data(server_io_ctx, endpoint_data), // create and open udp data socket
        start_at(ru_time::nowMicrosSystem()),      // start_at- (ru_time::nowMicrosSystem()
        idle_at(ru_time::nowMicrosSystem()),       // system micros desk became idle
        idle_shutdown(di.idle_shutdown)            // when to declare session idle
  {
    socket_ctrl.set_option(ip_tcp::no_delay(true));
    ESP_LOGI(TAG.data(), "sizeof=%u", sizeof(Session));
  }

public:
  ~Session() { ESP_LOGI("DeskSession", "falling out of scope"); }

  inline static bool active() {
    if (active::session.has_value() && active::session->socket_ctrl.is_open()) {
      // an active session exists and the socket is open
      return true;
    } else if (active::session.has_value()) {
      // there's a left over session and the socket is closed, deallocate it
      active::session.reset();
    }

    return false;
  }

  static void init(const session::Inject &di);

private:
  void data_msg_receive();
  void fps_calc();
  void handshake();
  void idle_watch_dog();
  void latency_feedback(int64_t async_us);
  bool send_ctrl_msg(const JsonDocument &doc);
  void shutdown();

private:
  // order dependent
  // NOTE:  all created sockets and timers use the Server io_ctx
  Elapsed uptime;
  io_context &server_io_ctx;
  tcp_socket socket_ctrl;
  steady_timer idle_timer;
  steady_timer fps_timer;
  udp_endpoint endpoint_data;
  udp_socket socket_data;

  // misc session times
  Micros start_at;
  Micros idle_at;
  Millis idle_shutdown;

  // order independent
  udp_endpoint endpoint_sender;
  std::unique_ptr<DMX> dmx;
  uint16_t msg_len = 0;
  static constexpr uint16_t MSG_LEN_SIZE = sizeof(msg_len);
  Packed packed{0};
  desk::stats stats;
};

} // namespace desk
} // namespace ruth
