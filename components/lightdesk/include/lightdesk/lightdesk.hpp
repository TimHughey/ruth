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

#ifndef _ruth_lightdesk_hpp
#define _ruth_lightdesk_hpp

#include <chrono>
#include <memory>

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include "dmx/dmx.hpp"
#include "misc/elapsed.hpp"

namespace lightdesk {

class LightDesk {
public:
  struct Opts {
    uint32_t dmx_port = 48005;
    uint32_t idle_shutdown_ms = 600000;
    uint32_t idle_check_ms = 1000;
  };

public:
  LightDesk(const Opts &opts);
  ~LightDesk() = default;

  void stop();
  void start();

  void idleWatch();
  static void idleWatchCallback(TimerHandle_t handle);
  void idleWatchDelete();

private:
  void init();

private:
  esp_err_t _init_rc = ESP_FAIL;
  TimerHandle_t _idle_timer = nullptr;
  uint32_t _idle_shutdown_ms = 600000;
  uint32_t _idle_check_ms = 1000; // one second
};

} // namespace lightdesk

#endif
