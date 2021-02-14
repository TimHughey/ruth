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

#include "lightdesk/fx/factory.hpp"
#include "lightdesk/headunits/ac_power.hpp"
#include "lightdesk/headunits/discoball.hpp"
#include "lightdesk/headunits/elwire.hpp"
#include "lightdesk/headunits/ledforest.hpp"
#include "lightdesk/headunits/pinspot/base.hpp"
#include "lightdesk/request.hpp"
#include "lightdesk/types.hpp"
#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "misc/random.hpp"
#include "protocols/dmx.hpp"
#include "protocols/i2s.hpp"
#include "protocols/payload.hpp"

namespace ruth {
namespace lightdesk {

typedef class LightDesk LightDesk_t;

class LightDesk {

public:
  LightDesk();
  ~LightDesk();

  float bassMagnitudeFloor() { return _i2s->bassMagnitudeFloor(); }

  void bassMagnitudeFloor(const float floor) {
    _i2s->bassMagnitudeFloor(floor);
  }

  float majorPeakMagFloor() { return _i2s->magFloor(); }
  void majorPeakMagFloor(const float floor) { _i2s->magFloor(floor); }

  static void preStart() {
    using TR = reading::Text;

    PulseWidthHeadUnit::preStart();

    TR::rlog("lightdesk enabled, prestart executed");
  }

  bool request(const Request_t &r);

  const LightDeskStats_t &stats();

private: // headunits
  inline DiscoBall_t *discoball() { return _discoball; }
  inline ElWire_t *elWireDanceFloor() { return _elwire[ELWIRE_DANCE_FLOOR]; }
  inline ElWire_t *elWireEntry() { return _elwire[ELWIRE_ENTRY]; }

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

private:
  void danceExecute();
  void danceStart(LightDeskMode_t mode);

  bool command(MsgPayload_t &msg);

  void framePrepare();

  void init();

  void majorPeakStart();

  // inline uint64_t secondsToInterval(const float secs) const {
  //   const uint64_t us_sec = 1000 * 1000;
  //   float integral;
  //   const float frac = modf(secs, &integral);
  //
  //   const uint64_t interval = (integral * us_sec) + (frac * us_sec);
  //   return interval;
  // }

  // static void timerCallback(void *data);
  // void timerDelete(esp_timer_handle_t &timer) {
  //   if (timer != nullptr) {
  //     esp_timer_handle_t x = timer;
  //     timer = nullptr;
  //     esp_timer_stop(x);
  //     esp_timer_delete(x);
  //   }
  // }
  //
  // uint64_t timerSchedule(float secs) const;

  // task
  void core();
  static void coreTask(void *task_instance);
  void start();
  void stopActual();

  inline TaskHandle_t task() const { return _task.handle; }
  inline const char *taskName() const { return pcTaskGetTaskName(nullptr); }
  bool taskNotify(NotifyVal_t val) const;

private:
  esp_err_t _init_rc = ESP_FAIL;

  Dmx_t *_dmx = nullptr;
  I2s_t *_i2s = nullptr;

  LightDeskMode_t _mode = INIT;
  Request_t _request = {};
  LightDeskStats_t _stats = {};

  AcPower_t *_ac_power = nullptr;
  PinSpot_t *_pinspots[2] = {};
  ElWire_t *_elwire[2] = {};
  LedForest_t *_led_forest = nullptr;
  DiscoBall_t *_discoball = nullptr;

  // map of 2d6 roll to fx patterns
  // note index 0 and 1 are impossible to roll but are included to
  // simplify the mapping and avoid index issues

  // 2d6 probabilities
  // 2: 2.78, 3: 5.56, 4: 8.33, 5: 11.11%, 6: 13.89%, 7: 16.67
  // 8: 13.89, 9: 11.11, 10: 9.33, 11: 5.56, 12: 2.78
  const FxType_t _fx_patterns[13] = {fxNone,            // 0
                                     fxNone,            // 1
                                     fxMajorPeak,       // 2
                                     fxMajorPeak,       // 3
                                     fxMajorPeak,       // 4
                                     fxMajorPeak,       // 5
                                     fxWashedSound,     // 6
                                     fxMajorPeak,       // 7
                                     fxFastStrobeSound, // 8
                                     fxMajorPeak,       // 9
                                     fxMajorPeak,       // 10
                                     fxMajorPeak,       // 11
                                     fxMajorPeak};      // 12

  const FxType_t _fx_patterns0[13] = {fxNone,       // 0
                                      fxNone,       // 1
                                      fxMajorPeak,  // 2
                                      fxMajorPeak,  // 3
                                      fxMajorPeak,  // 4
                                      fxMajorPeak,  // 5
                                      fxMajorPeak,  // 6
                                      fxMajorPeak,  // 7
                                      fxMajorPeak,  // 8
                                      fxMajorPeak,  // 9
                                      fxMajorPeak,  // 10
                                      fxMajorPeak,  // 11
                                      fxMajorPeak}; // 12

  const FxType_t _fx_patterns1[13] = {fxNone,                        // 0
                                      fxNone,                        // 1
                                      fxPrimaryColorsCycle,          // 2
                                      fxBlueGreenGradient,           // 3
                                      fxFullSpectrumCycle,           // 4
                                      fxSimpleStrobe,                // 5
                                      fxWashedSound,                 // 6
                                      fxMajorPeak,                   // 7
                                      fxFastStrobeSound,             // 8
                                      fxMajorPeak,                   // 9
                                      fxRgbwGradientFast,            // 10
                                      fxWhiteFadeInOut,              // 11
                                      fxGreenOnRedBlueWhiteJumping}; // 12

  // active Lightdesk Effect
  fx::FxBase *_fx = nullptr;
  fx::MajorPeak *_major_peak = nullptr;

  // task
  Task_t _task = {
      .handle = nullptr, .data = nullptr, .priority = 19, .stackSize = 4096};
};

} // namespace lightdesk
} // namespace ruth

#endif
