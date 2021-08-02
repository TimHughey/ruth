/*
    lightdesk/lightdesk.cpp - Ruth Light Desk
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

#include <esp_log.h>

#include "dmx/dmx.hpp"
#include "headunit/ac_power.hpp"
#include "headunit/discoball.hpp"
#include "headunit/elwire.hpp"
#include "headunit/headunit.hpp"
#include "headunit/ledforest.hpp"
#include "lightdesk/lightdesk.hpp"

using namespace dmx;

namespace lightdesk {

static const char *TAG = "lightdesk";
static dmx::Dmx *_dmx = nullptr;

LightDesk::LightDesk(const Opts &opts) {
  _idle_shutdown_ms = opts.idle_shutdown_ms;
  _idle_check_ms = opts.idle_check_ms;

  if (_dmx == nullptr) {
    _dmx = new Dmx(opts.dmx_port);
  }

  init();
  start();
}

IRAM_ATTR void LightDesk::idleWatch() {
  static auto track = esp_timer_get_time();

  if (_dmx->idle()) {
    auto now = esp_timer_get_time();

    auto idle_duration = now - track;

    if (idle_duration >= (_idle_shutdown_ms * 1000)) {
      dmx::HeadUnitTracker &headunits = _dmx->headunits();

      for (auto hu : headunits) {
        hu->dark();
      }
      track = now;
    }

  } else {
    track = esp_timer_get_time();
  }

  xTimerStart(_idle_timer, pdMS_TO_TICKS(_idle_check_ms));
}

IRAM_ATTR void LightDesk::idleWatchCallback(TimerHandle_t handle) {
  LightDesk *desk = (LightDesk *)pvTimerGetTimerID(handle);

  if (desk) desk->idleWatch();
}

void LightDesk::idleWatchDelete() {
  if (_idle_timer != nullptr) {
    if (xTimerIsTimerActive(_idle_timer) != pdFALSE) {
      if (xTimerStop(_idle_timer, pdMS_TO_TICKS(1000)) == pdFAIL) {
        goto FINISH;
      }
    }

    auto to_delete = _idle_timer;
    _idle_timer = nullptr;
    if (xTimerDelete(to_delete, pdMS_TO_TICKS(1000)) == pdFAIL) {
      _idle_timer = to_delete;
    }
  }

FINISH:
  if (_idle_timer) {
    ESP_LOGW(TAG, "lightdesk failed delete idle timer");
  }
}

void LightDesk::init() {
  ESP_LOGD(TAG, "enabled, starting up");

  _dmx->start();
  _dmx->addHeadUnit(std::make_shared<AcPower>());
  _dmx->addHeadUnit(std::make_shared<DiscoBall>(1)); // pwm 1
  _dmx->addHeadUnit(std::make_shared<ElWire>(2));    // pwm 2
  _dmx->addHeadUnit(std::make_shared<ElWire>(3));    // pwm 3
  _dmx->addHeadUnit(std::make_shared<LedForest>(4)); // pwm 4
}

void LightDesk::start() {
  _idle_timer = xTimerCreate("dmx_idle", pdMS_TO_TICKS(_idle_check_ms), pdFALSE, nullptr, &idleWatchCallback);
  vTimerSetTimerID(_idle_timer, this);
  xTimerStart(_idle_timer, pdMS_TO_TICKS(_idle_check_ms));
}

void LightDesk::stop() {
  idleWatchDelete();

  _dmx->stop();
}

} // namespace lightdesk
