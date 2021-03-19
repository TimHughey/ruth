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

#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "misc/random.hpp"
#include "protocols/dmx.hpp"

namespace ruth {
namespace lightdesk {

typedef class LightDesk LightDesk_t;

class LightDesk : public std::enable_shared_from_this<LightDesk> {

public:
  LightDesk() = default;
  ~LightDesk() = default;

  void start();
  void stop();

  void idleWatch();
  static void idleWatchCallback(TimerHandle_t handle);

  static std::shared_ptr<LightDesk> make_shared() {
    auto desk = std::make_shared<LightDesk>();

    desk->start();

    return std::move(desk);
  }

private:
  void init();

private:
  esp_err_t _init_rc = ESP_FAIL;

  std::shared_ptr<Dmx> _dmx;

  TimerHandle_t _idle_timer = nullptr;
  std::chrono::minutes _idle_shutdown = std::chrono::minutes(1);
  uint32_t _idle_check_ms = 15 * 1000; // 15 seconds
  bool _idle_timer_shutdown = false;
};

} // namespace lightdesk
} // namespace ruth

#endif
