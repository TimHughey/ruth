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

static const char *_fx_desc[] = {"fxNone",
                                 "fxPrimaryColorsCycle",
                                 "fxRedOnGreenBlueWhiteJumping",
                                 "fxGreenOnRedBlueWhiteJumping",
                                 "fxBlueOnRedGreenWhiteJumping",
                                 "fxWhiteOnRedGreenBlueJumping",
                                 "fxWhiteFadeInOut",
                                 "fxRgbwGradientFast",
                                 "fxRedGreenGradient",
                                 "fxRedBlueGradient",
                                 "fxBlueGreenGradient",
                                 "fxFullSpectrumCycle",
                                 "fxFullSpectrumJumping",
                                 "fxColorCycleSound",
                                 "fxColorStrobeSound",
                                 "fxFastStrobeSound",
                                 "fxUnkown", // 0x10
                                 "fxColorBars",
                                 "fxWashedSound",
                                 "fxSimpleStrobe",
                                 "fxCrossFadeFast"};

LightDesk *__singleton__ = nullptr;

LightDesk::LightDesk() {
  start();
  vTaskDelay(pdMS_TO_TICKS(100)); // allow time for tasks to start
}

LightDesk::~LightDesk() {
  LightDesk_t *desk = __singleton__;
  __singleton__ = nullptr;

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

  while (desk->_task.handle != nullptr) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void LightDesk::cleanUp() {
  TR::rlog("LightDesk shutdown");
  delete __singleton__;
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

    ready();
    rc = dance(interval);

    TR::rlog("LightDesk dance with interval=%3.2f %s", interval,
             (rc) ? "started" : "failed");
  }

  if (mode_obj) {
    const bool ready_flag = mode_obj["ready"];
    const bool stop_flag = mode_obj["stop"];

    if (ready_flag) {
      ready();
    }
    if (stop_flag) {
      // stop();
    }

    rc = true;
  }

  return rc;
}

void IRAM_ATTR LightDesk::core() {

  TR::rlog("LightDesk ready");

  while (_mode != SHUTDOWN) {
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

      case NotifyStop:
        _mode = STOP;

        if (_mode == STOP) {
          PulseWidth::off(1);

          Dmx_t *dmx = Dmx::instance();
          dmx->stop();

          taskNotify(NotifyShutdown);
        }
        break;

      case NotifyShutdown:
        _mode = SHUTDOWN;
        timerDelete(_dance_timer);
        timerDelete(_timer);
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

  TaskHandle_t task = lightdesk->_task.handle;
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
  if (secs < 0.10f) {
    secs = 0.10f;
  }

  const uint64_t interval = secondsToInterval(secs);
  esp_timer_start_once(_dance_timer, interval);

  return interval;
}

const char *LightDesk::fxDesc(Fx_t fx) { return _fx_desc[fx]; }

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

bool LightDesk::isRunning() {
  auto rc = true;

  if (__singleton__ == nullptr) {
    rc = false;
  }

  return rc;
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
  static const char *mode_str[] = {"ready",   "dance", "color",   "dark",
                                   "fade to", "stop",  "shutdown"};

  const auto mode_index = static_cast<uint32_t>(_mode);

  _stats.mode = mode_str[mode_index];
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
