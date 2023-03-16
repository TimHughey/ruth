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

#include "ArduinoJson.h"
#include "desk_msg/in.hpp"
#include "desk_msg/kv_store.hpp"
#include "io/io.hpp"
#include "ru_base/rut_types.hpp"
#include "ru_base/types.hpp"
#include "session/stats.hpp"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
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
  Session(io_context &io_ctx, tcp_socket &&sock) noexcept;

  ~Session() noexcept;

private:
  void close(const error_code ec = io::make_error()) noexcept;

  void idle_watch_dog() noexcept;

  void msg_loop(MsgIn &&msg_in) noexcept;
  void msg_process(MsgIn &&msg_in) noexcept;

  void post_stats() noexcept;
  static void report_stats(void *self_v) noexcept;

private:
  static void run_io_ctx(void *self_v) noexcept;

private:
  // order dependent
  io_context &io_ctx;
  tcp_socket data_sock;
  Millis idle_ms;        // initial default, may be overriden by handshake
  Millis stats_interval; // initial default, may be overriden by handshake
  esp_timer_handle_t stats_timer;
  esp_timer_handle_t destruct_timer;

  // order independent
  std::unique_ptr<DMX> dmx;
  std::size_t frame_len;

  // stats processing
  std::optional<desk::stats> stats;
  bool stats_pending{false};
  desk::kv_store stats_periodic;

  // class level to ensure there is a single task running
  // at any given time
  static TaskHandle_t th;

public:
  static constexpr const auto TAG{"Session"};
};

} // namespace desk
} // namespace ruth
