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
  if (_fx != nullptr) {
    delete _fx;
    _fx = nullptr;
  }

  for (uint32_t i = PINSPOT_MAIN; i < PINSPOT_NONE; i++) {
    PinSpot_t *pinspot = _pinspots[i];

    if (pinspot != nullptr) {
      _pinspots[i] = nullptr;
      pinspot->stop();
      delete pinspot;
    }
  }

  while (_task.handle != nullptr) {
    printf("LightDesk::~LightDesk() waiting for task to exit\n");
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // printf("LightDesk %p deleted\n", this);
}

void IRAM_ATTR LightDesk::core() {
  _mode = READY;

  while (_mode != SHUTDOWN) {
    uint32_t v = 0;
    auto notified =
        xTaskNotifyWait(0x00, ULONG_MAX, &v, pdMS_TO_TICKS(portMAX_DELAY));

    if (notified == pdTRUE) {
      NotifyVal_t notify = static_cast<NotifyVal_t>(v);

      PinSpot_t *pinspot = pinSpotObject(_request.func());

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
        danceStart();

        break;

      case NotifyDanceExecute:
        // prevent processing of pending timer events after a mode change
        if (_mode == DANCE) {
          danceExecute();
        }
        break;

      case NotifyStop:
        stopActual();
        break;

      case NotifyReady:
        TR::rlog("LightDesk ready");
        _mode = READY;
        break;

      case NotifyFadeTo:
        _mode = FADE_TO;
        pinspot->fadeTo(_request.fadeToRgbw(), _request.fadeToSecs());
        break;

      default:
        // no op
        break;
      }
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
  // NOTE:  when _mode is anything other than DANCE this function is a
  //        NOP and the dance execute timer is not rescheduled.

  if ((_mode == DANCE) && _fx) {
    auto fx_finished = _fx->execute();

    if (fx_finished == true) {
      _fx->execute(fxRandom());
    }

    danceTimerSchedule(_fx->nextTimerInterval());
  }
}

void LightDesk::danceStart() {
  esp_timer_stop(_dance_timer);

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
    _fx = new LightDeskFx(pinSpotMain(), pinSpotFill());
  }

  _fx->intervalDefault(_request.danceInterval());
  _fx->start();

  _mode = DANCE;

  TR::rlog("LightDesk dance with interval=%3.2f started",
           _request.danceInterval());

  danceTimerSchedule(_fx->nextTimerInterval());
}

void IRAM_ATTR LightDesk::danceTimerCallback(void *data) {
  LightDesk_t *lightdesk = (LightDesk_t *)data;

  lightdesk->taskNotify(NotifyDanceExecute);
}

uint64_t IRAM_ATTR LightDesk::danceTimerSchedule(float secs) const {

  // safety net, prevent runaway task
  if (secs < 0.10f) {
    secs = 0.10f;
  }

  const uint64_t interval = secondsToInterval(secs);
  esp_timer_start_once(_dance_timer, interval);

  return interval;
}

void LightDesk::init() {
  Dmx::start();

  // dmx address 1
  _pinspots[PINSPOT_MAIN] = new PinSpot(1, Dmx::frameInterval());

  // dmx address 7
  _pinspots[PINSPOT_FILL] = new PinSpot(7, Dmx::frameInterval());

  _timer_args = {
      .callback = timerCallback,
      .arg = this,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "lightd-timer",
  };

  esp_timer_create(&_timer_args, &_timer);

  _dance_timer_args = {
      .callback = danceTimerCallback,
      .arg = this,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "lightd-dance",
  };

  esp_timer_create(&_dance_timer_args, &_dance_timer);
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

  Dmx_t *dmx = Dmx::instance();
  dmx->stats(_stats.dmx);

  if (_fx) {
    _fx->stats(_stats.fx);
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
  _mode = STOP;

  PulseWidth::off(1);

  for (uint32_t i = PINSPOT_MAIN; i < PINSPOT_NONE; i++) {
    PinSpot_t *pinspot = _pinspots[i];

    if (pinspot) {
      pinspot->dark();
    }
  }

  // Dmx_t *dmx = Dmx::instance();
  // dmx->stop();
  //
  // timerDelete(_dance_timer);
  // timerDelete(_timer);
  TR::rlog("LightDesk stopped");
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

void LightDesk::timerCallback(void *data) {
  LightDesk_t *lightdesk = (LightDesk_t *)data;

  lightdesk->taskNotify(NotifyTimer);
}

uint64_t LightDesk::timerSchedule(float secs) const {
  const uint64_t us_sec = 1000 * 1000;
  float integral;
  const float frac = modf(secs, &integral);

  const uint64_t interval = (integral * us_sec) + (frac * us_sec);

  esp_timer_start_once(_timer, interval);

  return interval;
}

} // namespace lightdesk
} // namespace ruth
