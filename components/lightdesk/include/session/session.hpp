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
public:
  static constexpr csv TAG{"Session"};

private:
  // use init() to construct a new Session
  inline Session(const session::Inject &di) // create the session
      : local_ref_time(rut::raw()),         // for calculating local system time diffs
        server_io_ctx(di.io_ctx),           // server io_ctx (for session cleanup)
        socket_ctrl(std::move(di.socket)),  // move the socket connection accepted
        idle_timer(server_io_ctx),          // idle timer
        stats_timer(server_io_ctx),         // frames per second timer
        idle_shutdown(di.idle_shutdown),    // when to declare session idle
        stats_interval(2000),               // frequency to calculate stats
        stats(stats_interval)               // initialize the stats object
  {
    socket_ctrl.set_option(ip_tcp::no_delay(true));
    handshake_part1();
  }

public:
  ~Session() { ESP_LOGI(TAG.data(), "falling out of scope"); }

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
  void data_feedback(const JsonDocument &data_doc, const int64_t async_us, Elapsed &elapsed);
  void data_msg_rx();
  void connect_data(Port port);
  void fps_calc();
  void handshake_part1();
  void handshake_part2();
  void idle_watch_dog();

  inline bool success(const error_code ec) {
    if (ec) {
      ESP_LOGW(TAG.data(), "shutdown, reason=%s", ec.message().c_str());
      shutdown();
    }

    return !ec;
  }

  void shutdown();

  // misc debug, logging
  inline void log_feedback(const error_code &ec) {

    if (ec) {
      ESP_LOGW(TAG.data(), "async_write_msg() failed, reason: %s", ec.message().c_str());

      shutdown();
    }
  }

private:
  // order dependent
  // NOTE:  all created sockets and timers use the Server io_ctx
  const Micros local_ref_time; // system micros
  io_context &server_io_ctx;
  tcp_socket socket_ctrl;
  steady_timer idle_timer;
  steady_timer stats_timer;
  Millis idle_shutdown;
  Millis stats_interval;
  desk::stats stats;

  // order independent
  std::optional<tcp_socket> socket_data;

  // time keeping
  Micros remote_ref_time;

  // order independent
  std::unique_ptr<DMX> dmx;
  uint16_t msg_len = 0;
};

} // namespace desk
} // namespace ruth
