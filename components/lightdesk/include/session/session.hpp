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
#include "io/io.hpp"
#include "ru_base/types.hpp"

#include <array>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <memory>

namespace ruth {

namespace desk {

class DMX;
class MsgIn;
class Session;

class Session {

private:
  Session(io_context &io_ctx, tcp_socket &&peer) noexcept;

public:
  ~Session() noexcept;

  static void create(io_context &io_ctx, tcp_socket &&sock) noexcept;

  inline void close_any() noexcept {
    std::for_each(sessions.begin(), sessions.end(), [](auto &sess) {
      if (sess) sess.reset();
    });
  }

private:
  void close(const error_code ec = io::make_error()) noexcept;

  void idle_watch_dog() noexcept;

  void data_msg_loop(MsgIn &&msg_in) noexcept;
  void data_msg_process(MsgIn &&msg_in) noexcept;

  void sess_msg_loop(MsgIn &&msg_in) noexcept;
  void sess_msg_process(MsgIn &&msg_in) noexcept;

  static void report_stats(void *self_v) noexcept;

private:
  static inline auto &active() noexcept { return sessions.front(); }
  static void idle_timeout(void *self_v) noexcept;
  static void self_destruct(TimerHandle_t timer) noexcept;
  static void run_io_ctx(void *self_v) noexcept;

private:
  // order dependent
  io_context &io_ctx;
  tcp_socket sess_sock;
  tcp_socket data_sock;

  // order independent
  int idle_us{10'000 * 1000};              // default, may be overriden by handshake
  std::size_t frame_len{14};               // default, may be overriden by handshake
  esp_timer_handle_t idle_timer{nullptr};  // esp timer to detect idle timeout
  esp_timer_handle_t stats_timer{nullptr}; // esp timer to report stats
  std::unique_ptr<DMX> dmx;                // dmx object / task
  TaskHandle_t th{nullptr};                // session task

  // class level tracking of at most two sessions
  static std::array<std::unique_ptr<Session>, 2> sessions;

public:
  static constexpr const auto TAG{"Session"};
};

} // namespace desk
} // namespace ruth
