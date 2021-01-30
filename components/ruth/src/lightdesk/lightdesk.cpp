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
#include <math.h>

#include "engines/pwm.hpp"
#include "external/ArduinoJson.h"
#include "lightdesk/lightdesk.hpp"
#include "protocols/dmx.hpp"
#include "readings/text.hpp"

namespace ruth {
namespace lightdesk {

using TR = reading::Text;

LightDesk::LightDesk() {
  start();
  vTaskDelay(pdMS_TO_TICKS(100)); // allow time for tasks to start
}

LightDesk::~LightDesk() {
  while (_task.handle != nullptr) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  if (_i2s != nullptr) {
    delete _i2s;
    _i2s = nullptr;
  }

  if (_dmx != nullptr) {
    delete _dmx;
    _dmx = nullptr;
  }

  if (_fx != nullptr) {
    delete _fx;
    _fx = nullptr;
  }

  for (uint32_t i = PINSPOT_MAIN; i < PINSPOT_NONE; i++) {
    PinSpot_t *pinspot = _pinspots[i];

    if (pinspot != nullptr) {
      _pinspots[i] = nullptr;

      delete pinspot;
    }
  }

  TR::rlog("%s complete", __PRETTY_FUNCTION__);
}

void IRAM_ATTR LightDesk::core() {
  _mode = READY;

  while (_mode != SHUTDOWN) {
    bool stream_frames = true;
    uint32_t v = 0;
    auto notified = xTaskNotifyWait(0x00, ULONG_MAX, &v, pdMS_TO_TICKS(100));

    if (notified == pdTRUE) {
      NotifyVal_t notify = static_cast<NotifyVal_t>(v);

      PinSpot_t *pinspot = pinSpotObject(_request.func());

      // ensure DMX is streaming frames unless we're stopping or shutting down
      if ((notify == NotifyStop) || (notify == NotifyShutdown)) {
        stream_frames = false;
      }

      switch (notify) {

      case NotifyColor:
        _mode = COLOR;
        pinspot->color(_request.colorRgbw(), _request.colorStrobe());
        break;

      case NotifyDark:
        _mode = DARK;
        pinSpotMain()->dark();
        pinSpotFill()->dark();
        break;

      case NotifyDance:
        _mode = READY;
        danceStart(DANCE);
        break;

      case NotifyStop:
        stopActual();
        break;

      case NotifyShutdown:
        _mode = SHUTDOWN;
        break;

      case NotifyReady:
        TR::rlog("LightDesk ready");
        _mode = READY;
        break;

      case NotifyFadeTo:
        _mode = FADE_TO;
        pinspot->fadeTo(_request.fadeToRgbw(), _request.fadeToSecs());
        break;

      case NotifyMajorPeak:
        _mode = READY;
        danceStart(MAJOR_PEAK);
        break;

      case NotifyPrepareFrame:
        framePrepare();
        break;

      default:
        stream_frames = false;
        break;
      }

      _dmx->streamFrames(stream_frames);
    }
  }
}

void LightDesk::coreTask(void *task_instance) {
  LightDesk_t *lightdesk = (LightDesk_t *)task_instance;

  lightdesk->init();
  lightdesk->core();

  TaskHandle_t task = lightdesk->_task.handle;
  TR::rlog("LightDesk task %p exiting", task);
  lightdesk->_task.handle = nullptr;

  vTaskDelete(task); // task removed from the scheduler
}

void IRAM_ATTR LightDesk::danceExecute() {
  // NOTE:  when _mode is anything other than DANCE or MAJOR_PEAL  this
  // function is a NOP and the dance execute timer is not rescheduled.

  if (((_mode == MAJOR_PEAK) || (_mode == DANCE)) && _fx) {
    auto fx_finished = _fx->execute();

    if (fx_finished == true) {
      _fx->execute(fxRandom());
    }
  }
}

void LightDesk::danceStart(LightDeskMode_t mode) {

  // turn on the PulseWidth device that controls the head unit power
  PulseWidth::on(1);
  pinSpotMain()->dark();
  pinSpotFill()->dark();

  // anytime the LightDesk is requested to start DANCE mode
  // completely reset the LightDeskFx controller.
  if (_fx) {
    delete _fx;
    _fx = nullptr;
  }

  if (_fx == nullptr) {
    _fx = new LightDeskFx(pinSpotMain(), pinSpotFill(), _i2s);
  }

  if (mode == DANCE) {
    _fx->intervalDefault(_request.danceInterval());
  }

  _fx->start();

  _mode = mode;

  if (_mode == DANCE) {
    TR::rlog("LightDesk dance with interval=%3.2f started",
             _request.danceInterval());
  }
}

void IRAM_ATTR LightDesk::framePrepare() {
  elapsedMicros e;

  danceExecute();

  pinSpotMain()->framePrepare();
  pinSpotFill()->framePrepare();

  e.freeze();

  const uint_fast32_t duration = (uint32_t)e;
  uint_fast32_t &min = _stats.frame_prepare.min;
  uint_fast32_t &curr = _stats.frame_prepare.curr;
  uint_fast32_t &max = _stats.frame_prepare.max;

  min = (duration < min) ? duration : min;
  curr = duration;
  max = (duration > max) ? duration : max;
}

void LightDesk::init() {

  _i2s = (_i2s == nullptr) ? new I2s() : _i2s;
  _dmx = (_dmx == nullptr) ? new Dmx() : _dmx;

  _i2s->start();
  vTaskDelay(pdMS_TO_TICKS(300)); // allow time for i2s to start

  _dmx->start();

  // dmx address 1
  _pinspots[PINSPOT_MAIN] = new PinSpot(1);
  _dmx->registerHeadUnit(_pinspots[PINSPOT_MAIN]);

  // dmx address 7
  _pinspots[PINSPOT_FILL] = new PinSpot(7);
  _dmx->registerHeadUnit(_pinspots[PINSPOT_FILL]);

  _dmx->prepareTaskRegister(task());
}

uint32_t LightDesk::randomInterval(uint32_t min, uint32_t max) const {
  const uint32_t modulo = max - min;
  const uint32_t interval = (esp_random() % modulo) + min;

  return interval;
}

bool LightDesk::request(const Request_t &r) {
  auto rc = true;
  _request = r;

  switch (r.mode()) {
  case COLOR:
    rc = taskNotify(NotifyColor);
    break;

  case DANCE:
    rc = taskNotify(NotifyDance);
    break;

  case DARK:
    rc = taskNotify(NotifyDark);
    break;

  case FADE_TO:
    rc = taskNotify(NotifyFadeTo);
    break;

  case MAJOR_PEAK:
    rc = taskNotify(NotifyMajorPeak);
    break;

  case READY:
    rc = taskNotify(NotifyReady);
    break;

  case STOP:
    rc = taskNotify(NotifyStop);
    break;

  default:
    rc = false;
    break;
  }

  return rc;
}

void LightDesk::start() {

  if (_task.handle == nullptr) {
    // this (object) is passed as the data to the task creation and is
    // used by the static coreTask method to call cpre()
    ::xTaskCreate(&coreTask, "lightdesk", _task.stackSize, this, _task.priority,
                  &(_task.handle));
  }
}

const LightDeskStats_t &LightDesk::stats() {
  _stats.mode = modeDesc(_mode);
  _stats.object_size = sizeof(LightDesk_t);

  if (_dmx) {
    _dmx->stats(_stats.dmx);
  }

  if (_fx) {
    _fx->stats(_stats.fx);
  }

  if (_i2s) {
    _i2s->stats(_stats.i2s);
  }

  const uint32_t last_pinspot = static_cast<uint32_t>(PINSPOT_FILL);
  for (auto i = 0; i <= last_pinspot; i++) {
    PinSpot_t *pinspot = _pinspots[i];
    PinSpotStats_t &stats = _stats.pinspot[i];

    pinspot->stats(stats);
  }

  return _stats;
}

void LightDesk::stopActual() {
  LightDeskMode_t prev_mode = _mode;
  _mode = STOP;

  _dmx->prepareTaskUnregister();

  PulseWidth::off(1);

  for (uint32_t i = PINSPOT_MAIN; i < PINSPOT_NONE; i++) {
    PinSpot_t *pinspot = _pinspots[i];

    if (pinspot) {
      pinspot->dark();
    }
  }

  vTaskDelay(10); // allow time for PinSpots to set dark

  if (_fx) {
    _fx->stop();
  }

  if (_dmx) {
    _dmx->stop();
  }

  if (_i2s) {
    _i2s->stop();
  }

  if (prev_mode != STOP) {
    TR::rlog("LightDesk stopped");
  }

  taskNotify(NotifyShutdown);
}

bool IRAM_ATTR LightDesk::taskNotify(NotifyVal_t val) const {
  bool rc = true;
  const uint32_t v = static_cast<uint32_t>(val);

  if (task()) { // prevent attempts to notify the task that does not exist
    xTaskNotify(task(), v, eSetValueWithOverwrite);
  } else {
    TR::rlog("LightDesk: attempt to send NotifyVal_t %u to null task handle",
             val);
    rc = false;
  }

  return rc;
}

} // namespace lightdesk
} // namespace ruth
