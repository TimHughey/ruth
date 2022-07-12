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

#include "base/ru_time.hpp"
#include "io/io.hpp"
#include "misc/elapsed.hpp"
#include "state/state.hpp"

#include <chrono>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <memory>

namespace ruth {

class LightDesk;
typedef std::shared_ptr<LightDesk> shLightDesk;

class LightDesk : public std::enable_shared_from_this<LightDesk> {
public:
  struct Opts {
    uint32_t dmx_port = 48005;
    Millis idle_shutdown = ru_time::as_duration<Seconds, Millis>(600s);
    Millis idle_check = ru_time::as_duration<Seconds, Millis>(1s);
  };

private:
  LightDesk(const Opts &opts)
      : endpoint(ip_udp::v4(), 0),            //  udp endpoint (any available port)
        socket(io_ctx, endpoint),             //  the msg socket
        idle_timer(io_ctx),                   // idle timer
                                              // dmx(new DMX(io_ctx)),
        start_at(ru_time::nowMicrosSystem()), // start_at- (ru_time::nowMicrosSystem()
        idle_at(ru_time::nowMicrosSystem()),  // system micros desk became idle
        idle_check(opts.idle_check)           // how often to check desk is idle
  {}

public: // static function to create, access and reset shared LightDesk
  static shLightDesk create(const Opts &opts);
  static shLightDesk ptr();
  static void reset(); // reset (deallocate) shared instance

  // general public API
  shLightDesk init(); // starts LightDesk task

  static void stop() {
    [[maybe_unused]] error_code ec;
    auto self = ptr();

    self->socket.close(ec);

    self->idle_timer.cancel();
    self->io_ctx.stop();
  }

private:
  void idleWatchDog();
  void messageLoop();
  void run();

private:
  // order dependent
  io_context io_ctx;
  udp_endpoint endpoint;
  udp_socket socket;
  steady_timer idle_timer;
  // upDMX dmx;
  Micros start_at;
  Micros idle_at;
  Millis idle_check; // watchdog timeout to declare LightDesk idle

  // order independent
  udp_endpoint remote_endpoint;
  desk::State state;
};

} // namespace ruth
