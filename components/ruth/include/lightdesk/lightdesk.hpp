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

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include "devs/dmx/pinspot/base.hpp"
#include "lightdesk/fx.hpp"
#include "lightdesk/fx_defs.hpp"
#include "lightdesk/types.hpp"
#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "misc/random.hpp"
#include "protocols/dmx.hpp"
#include "protocols/payload.hpp"

namespace ruth {
namespace lightdesk {

typedef class LightDesk LightDesk_t;

typedef enum { PINSPOT_MAIN = 0, PINSPOT_FILL, PINSPOT_NONE } PinSpotFunction_t;

class LightDesk {
private:
  typedef enum {
    PAUSED = 0x00,
    READY,
    DANCE,
    COLOR,
    DARK,
    FADE_TO
  } LightDeskMode_t;

  typedef struct {
    PinSpotFunction_t func = PINSPOT_NONE;

    union {
      struct {
        uint32_t secs;
      } dance;

      struct {
        uint32_t rgbw;
        float strobe;
      } color;

      struct {
        uint32_t rgbw;
        float secs;
      } fadeto;
    };

  } Request_t;

public:
  void color(PinSpotFunction_t func, uint32_t rgbw, float strobe = 0.0) {
    _request.func = func;
    _request.color.rgbw = rgbw;
    _request.color.strobe = strobe;

    taskNotify(NotifyColor);
  }

  bool dance(const float secs) {
    requestClear();
    _request.dance.secs = secs;

    return taskNotify(NotifyDance);
  }

  void dark() {
    requestClear();
    taskNotify(NotifyDark);
  }

  static bool externalCommand(MsgPayload_t &msg) {
    return instance()->command(msg);
  }

  void fadeTo(PinSpotFunction_t func, uint32_t rgbw, float secs = 2.5) {
    _request.func = func;
    _request.fadeto.rgbw = rgbw;
    _request.fadeto.secs = secs;
    taskNotify(NotifyFadeTo);
  }

  static uint64_t frameInterval() { return Dmx::frameInterval(); }

  static LightDesk_t *instance();

  void pause() { taskNotify(NotifyPause); }
  void resume() { taskNotify(NotifyResume); }

  const LightDeskStats_t &stats();

private:
  LightDesk(); // singleton

  void danceExecute();
  void danceStart();
  static void danceTimerCallback(void *data);
  uint64_t danceTimerSchedule(float secs) const;

  void deskPause();
  void deskResume();

  bool command(MsgPayload_t &msg);

  inline Fx_t fxRandom() const {
    const uint8_t roll = diceRoll();
    const Fx_t choosen_fx = _fx_patterns[roll];

    return choosen_fx;
  }

  void init();

  inline PinSpot *pinSpotObject(PinSpotFunction_t func) {
    PinSpot_t *spot = nullptr;

    if (func != PINSPOT_NONE) {
      const uint32_t index = static_cast<uint32_t>(func);
      spot = _pinspots[index];
    }

    return spot;
  }

  inline PinSpot_t *pinSpotMain() { return pinSpotObject(PINSPOT_MAIN); }
  inline PinSpot_t *pinSpotFill() { return pinSpotObject(PINSPOT_FILL); }

  inline uint32_t randomInterval(uint32_t min, uint32_t max) const;

  inline void requestClear() {
    _request = {};
    _request.func = PINSPOT_NONE;
  }

  inline uint64_t secondsToInterval(const float secs) const {
    const uint64_t us_sec = 1000 * 1000;
    float integral;
    const float frac = modf(secs, &integral);

    const uint64_t interval = (integral * us_sec) + (frac * us_sec);
    return interval;
  }

  uint64_t timerSchedule(float secs) const;
  static void timerCallback(void *data);

  // task
  void core();
  static void coreTask(void *task_instance);
  void start();

  inline TaskHandle_t task() const { return _task.handle; }
  inline const char *taskName() const { return pcTaskGetTaskName(nullptr); }
  inline BaseType_t taskNotify(NotifyVal_t val) const {
    const uint32_t v = static_cast<uint32_t>(val);
    return xTaskNotify(task(), v, eSetValueWithOverwrite);
  }

private:
  esp_err_t _init_rc = ESP_FAIL;
  bool _running = true;

  LightDeskMode_t _mode = READY;
  Request_t _request = {};
  LightDeskStats_t _stats = {};

  PinSpot_t *_pinspots[2];

  // map of 2d6 roll to fx patterns
  // note index 0 and 1 are impossible to roll but are included to
  // simplify the mapping and avoid index issues

  // 2d6 probabilities
  // 2: 2.78, 3: 5.56, 4: 8.33, 5: 11.11%, 6: 13.89%, 7: 16.67
  // 8: 13.89, 9: 11.11, 10: 9.33, 11: 5.56, 12: 2.78
  const Fx_t _fx_patterns[13] = {fxDark,               // 0
                                 fxDark,               // 1
                                 fxPrimaryColorsCycle, // 2
                                 fxBlueGreenGradient,  // 3
                                 fxFullSpectrumCycle,  // 4
                                 fxRedBlueGradient,    // 5
                                 fxWashedSound,        // 6
                                 fxFastStrobeSound,    // 7
                                 fxSimpleStrobe,       // 8
                                 fxCrossFadeFast,      // 9
                                 fxRgbwGradientFast,   // 10
                                 fxWhiteFadeInOut,     // 11
                                 fxSimpleStrobe};      // 12

  // light desk fx controller
  LightDeskFx_t *_fx = nullptr;

  // dance execute timer
  esp_timer_handle_t _dance_timer = nullptr;
  esp_timer_create_args_t _dance_timer_args;

  // high-res timers
  esp_timer_handle_t _timer = nullptr;
  esp_timer_create_args_t _timer_args;
  float _timer_interval = 3.0f;

  // task
  Task_t _task = {
      .handle = nullptr, .data = nullptr, .priority = 19, .stackSize = 4096};
}; // namespace ruth

} // namespace lightdesk
} // namespace ruth

#endif
