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

#include "io/io.hpp"
#include "msg/in.hpp"
#include "ru_base/rut_types.hpp"
#include "ru_base/types.hpp"
#include "session/stats.hpp"

#include "ArduinoJson.h"
#include <esp_log.h>
#include <memory>
#include <optional>

namespace ruth {

class DMX;

namespace desk {

class Session;
}

namespace shared {
extern std::optional<desk::Session> active_session;
}

namespace desk {

class Session {

public:
  // use init() to construct a new Session
  Session(tcp_socket &&sock) noexcept;

  ~Session() noexcept;

private:
  void close(const error_code ec = io::make_error()) noexcept;
  void connect_data(Port port) noexcept;

  void ctrl_msg_process(MsgIn &&msg) noexcept;

  void data_msg_reply(MsgIn &&msg) noexcept;

  void handshake() noexcept;

  void idle_watch_dog() noexcept;

  void read_ctrl_msg(MsgIn &&msg) noexcept;
  void read_data_msg(MsgIn &&msg) noexcept;

  void report_stats() noexcept;

private:
  // order dependent
  // NOTE:  all created sockets and timers use the socket executor
  tcp_socket ctrl_sock;
  tcp_socket data_sock;
  Millis idle_shutdown;  // initial default, may be overriden by handshake
  Millis stats_interval; // initial default, may be overriden by handshake
  steady_timer stats_timer;
  esp_timer_handle_t destruct_timer;

  // order independent
  std::unique_ptr<DMX> dmx;
  std::optional<desk::stats> stats;
  uint16_t msg_len{0};

public:
  static constexpr const auto TAG{"Session"};
};

} // namespace desk
} // namespace ruth
