/*
    lightdesk/lightdesk.hpp - Ruth Light Desk
    Copyright (C) 2020  Tim Hughey

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

#include "io/io.hpp"
#include "ru_base/time.hpp"
#include "state/state.hpp"

#include <chrono>
#include <memory>

namespace ruth {

class LightDesk;
typedef std::shared_ptr<LightDesk> shLightDesk;

class LightDesk : public std::enable_shared_from_this<LightDesk> {
public:
  static constexpr csv TAG = "lightdesk";

public:
  struct Opts {
    Millis idle_shutdown = ru_time::as_duration<Seconds, Millis>(15s);
    Millis idle_check = ru_time::as_duration<Seconds, Millis>(1s);
  };

private:
  // LightDesk(const Opts &opts)
  //     : endpoint(ip_udp::v4(), 0),            // udp endpoint (any available port)
  //       socket(io_ctx, endpoint),             // the msg socket
  //       idle_timer(io_ctx),                   // idle timer
  //       start_at(ru_time::nowMicrosSystem()), // start_at- (ru_time::nowMicrosSystem()
  //       idle_at(ru_time::nowMicrosSystem()),  // system micros desk became idle
  //       idle_check(opts.idle_check)           // how often to check desk is idle
  // {}

  LightDesk(const Opts &opts) : opts(opts) {}

public: // static function to create, access and reset shared LightDesk
  static shLightDesk create(const Opts &opts);
  static shLightDesk ptr();
  static void reset(); // reset (deallocate) shared instance

  // general public API
  shLightDesk init(); // starts LightDesk task

  static void stop() {
    // [[maybe_unused]] error_code ec;
    auto self = ptr();

    // self->socket.close(ec);
    // self->idle_timer.cancel();
    self->io_ctx.stop();
  }

private:
  // void idleWatchDog();
  // void messageLoop();

  static inline void _run([[maybe_unused]] void *data) { ptr()->run(); }
  void run(); // see .cpp file

private:
  // order dependent
  io_context io_ctx;
  const Opts opts;
  // udp_endpoint endpoint;
  // udp_socket socket;
  // steady_timer idle_timer;
  // Micros start_at;
  // Micros idle_at;
  // Millis idle_check; // watchdog timeout to declare LightDesk idle

  // order independent
  // udp_endpoint remote_endpoint;
  desk::State state;

  static constexpr Port SERVICE_PORT = 49152;
};

} // namespace ruth
