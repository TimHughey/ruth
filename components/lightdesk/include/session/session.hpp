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

#include "inject/inject.hpp"
#include "io/io.hpp"
#include "ru_base/types.hpp"
#include "ru_base/uint8v.hpp"

#include "ArduinoJson.h"
#include <esp_log.h>
#include <memory>
#include <optional>

namespace ruth {
namespace desk {

class Session; // forward decl for shared_ptr def
typedef std::shared_ptr<Session> shSession;

class Session : public std::enable_shared_from_this<Session> {
public:
  static void start(const session::Inject &di);

private:
  Session(const session::Inject &di)          // create the session
      : socket(std::move(di.socket)),         // move the socket connection accepted
        idle_timer(di.io_ctx),                // idle timer
        start_at(ru_time::nowMicrosSystem()), // start_at- (ru_time::nowMicrosSystem()
        idle_at(ru_time::nowMicrosSystem()),  // system micros desk became idle
        idle_check(di.idle_check),            // frequency of idle check timer
        idle_shutdown(di.idle_shutdown)       // when to declare session idle
  {}

public:
  ~Session() {
    ESP_LOGI("DeskSession", "falling out of scope, handle=%d", socket.native_handle());

    [[maybe_unused]] error_code ec; // must use error_code overload to prevent throws
    socket.cancel(ec);
    idle_timer.cancel(ec);
    socket.shutdown(tcp_socket::shutdown_both, ec);
    socket.close(ec);
  }

  static shSession activeSession();

private:
  void idleWatchDog();

private:
  // order dependent
  tcp_socket socket;
  steady_timer idle_timer;
  Micros start_at;
  Micros idle_at;
  Millis idle_check; // watchdog timeout to declare LightDesk idle
  Seconds idle_shutdown;
};

} // namespace desk
} // namespace ruth
