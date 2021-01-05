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

LightDesk *__singleton__ = nullptr;

LightDesk::LightDesk() {
  start();
  vTaskDelay(pdMS_TO_TICKS(100)); // allow time for tasks to start
}

bool LightDesk::command(MsgPayload_t &msg) {
  bool rc = false;
  StaticJsonDocument<256> doc;

  auto err = deserializeMsgPack(doc, msg.data());

  auto root = doc.as<JsonObject>();

  if (err) {
    TR::rlog("LightDesk command parse failure: %s", err.c_str());
    return rc;
  }

  auto dance_obj = root["dance"].as<JsonObject>();
  auto mode_obj = root["mode"].as<JsonObject>();

  if (dance_obj) {
    const float interval = dance_obj["interval_secs"];

    resume();
    rc = dance(interval);

    TR::rlog("LightDesk dance with interval=%3.2f %s", interval,
             (rc) ? "started" : "failed");
  }

  if (mode_obj) {
    const bool ready_flag = mode_obj["ready"];
    const bool pause_flag = mode_obj["pause"];

    if (ready_flag) {
      resume();
    }
    if (pause_flag) {
      pause();
    }

    rc = true;
  }

  return rc;
}

void IRAM_ATTR LightDesk::core() {
  while (_running) {
    uint32_t v = 0;
    auto notified =
        xTaskNotifyWait(0x00, ULONG_MAX, &v, pdMS_TO_TICKS(portMAX_DELAY));

    if (notified == pdTRUE) {
      NotifyVal_t notify = static_cast<NotifyVal_t>(v);

      PinSpot_t *pinspot = pinSpotObject(_request.func);

      switch (notify) {

      case NotifyColor:
        _mode = COLOR;
        pinspot->color(_request.color.rgbw, _request.color.strobe);
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

      case NotifyPause:
        deskPause();
        break;

      case NotifyResume:
        deskResume();
        break;

      case NotifyReady:
        _mode = READY;
        break;

      case NotifyFadeTo:
        _mode = FADE_TO;
        pinspot->fadeTo(_request.fadeto.rgbw, _request.fadeto.secs);
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

  lightdesk->pause();
  Dmx::shutdown();

  // if the core task ever returns then wait forever
  vTaskDelay(UINT32_MAX);
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
    _fx->intervalDefault(_request.dance.secs);
    _fx->start();

    _mode = DANCE;

    danceTimerSchedule(_fx->nextTimerInterval());
  }
}

void IRAM_ATTR LightDesk::danceTimerCallback(void *data) {
  LightDesk_t *lightdesk = (LightDesk_t *)data;

  lightdesk->taskNotify(NotifyDanceExecute);
}

uint64_t IRAM_ATTR LightDesk::danceTimerSchedule(float secs) const {

  // safety net, prevent runaway task
  if (secs < 0.90f) {
    secs = 0.90f;
  }

  const uint64_t interval = secondsToInterval(secs);
  esp_timer_start_once(_dance_timer, interval);

  return interval;
}

void LightDesk::deskPause() {
  if (_mode != PAUSED) {
    PulseWidth::off(1);

    Dmx_t *dmx = Dmx::instance();

    _pinspots[0]->pause();
    _pinspots[1]->pause();

    dmx->pause();

    TR::rlog("LightDesk paused");

    _mode = PAUSED;
  }
}

void LightDesk::deskResume() {
  if (_mode == PAUSED) {
    Dmx_t *dmx = Dmx::instance();

    _pinspots[0]->resume();
    _pinspots[1]->resume();

    dmx->resume();

    TR::rlog("LightDesk ready");

    taskNotify(NotifyReady);
  }
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

LightDesk_t *LightDesk::instance() {
  if (__singleton__ == nullptr) {
    __singleton__ = new LightDesk();
  }

  return __singleton__;
}

uint32_t LightDesk::randomInterval(uint32_t min, uint32_t max) const {
  const uint32_t modulo = max - min;
  const uint32_t interval = (esp_random() % modulo) + min;

  return interval;
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
  static const char *mode_str[6] = {"paused", "ready", "dance",
                                    "color",  "dark",  "fade to"};

  const auto mode_index = static_cast<uint32_t>(_mode);

  _stats.mode = mode_str[mode_index];

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
