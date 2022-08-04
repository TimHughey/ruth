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
#include <esp_timer.h>
#include <memory>

namespace ruth {

class LightDesk;
typedef std::shared_ptr<LightDesk> shLightDesk;

class LightDesk : public std::enable_shared_from_this<LightDesk> {
public:
  static constexpr csv TAG = "lightdesk";

public:
  struct Opts {
    Millis idle_shutdown = ru_time::as_duration<Seconds, Millis>(120s);
  };

private:
  LightDesk(const Opts &opts) : opts(opts) {}

public: // static function to create, access and reset shared LightDesk
  static shLightDesk create(const Opts &opts);
  static shLightDesk ptr();
  static void reset(); // reset (deallocate) shared instance

  // general public API
  shLightDesk init(); // starts LightDesk task

private:
  static inline void _run([[maybe_unused]] void *data) { ptr()->run(); }
  void run(); // see .cpp file

private:
  // order dependent
  io_context io_ctx;
  const Opts opts;

  // order independent

  desk::State state;

  static constexpr Port SERVICE_PORT = 49152;
};

} // namespace ruth
