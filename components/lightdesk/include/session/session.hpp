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
extern std::shared_ptr<Session> session;
}

class Session : public std::enable_shared_from_this<Session> {

private:
  // use init() to construct a new Session
  inline Session(const session::Inject &di) // create the session
      : server_io_ctx(di.io_ctx),           // server io_ctx (for session cleanup)
        ctrl_sock(std::move(di.socket)),    // move the socket connection accepted
        idle_timer(server_io_ctx),          // idle timer
        stats_timer(server_io_ctx),         // frames per second timer
        idle_shutdown(di.idle_shutdown),    // when to declare session idle
        stats_interval(2s),                 // frequency to calculate stats
        stats(stats_interval)               // initialize the stats object
  {}

public:
  ~Session() = default;

  static std::shared_ptr<Session> init(const session::Inject &di);

private:
  void ctrl_msg_loop() noexcept;
  void data_msg_loop() noexcept;
  void connect_data(Port port) noexcept;
  void fps_calc() noexcept;

  // kick off the session, the shared_ptr is passed to handlers keeping
  // the session in memory
  static void handshake(std::shared_ptr<Session> session) noexcept;

  void idle_watch_dog() noexcept;

private:
  // order dependent
  // NOTE:  all created sockets and timers use the Server io_ctx
  io_context &server_io_ctx;
  tcp_socket ctrl_sock;
  system_timer idle_timer;
  system_timer stats_timer;
  Millis idle_shutdown;
  Millis stats_interval;
  desk::stats stats;

  // order independent
  std::optional<tcp_socket> data_sock;

  // time keeping
  Micros remote_ref_time;

  // order independent
  std::unique_ptr<DMX> dmx;
  uint16_t msg_len = 0;

public:
  static constexpr csv TAG{"Session"};
};

} // namespace desk
} // namespace ruth
