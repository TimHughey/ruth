/*
    lightdesk/headunits/pwm_base.hpp - Ruth LightDesk Head Unit Pwm Base
    Copyright (C) 2021  Tim Hughey

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

#ifndef _ruth_lightdesk_headunits_pwm_base_hpp
#define _ruth_lightdesk_headunits_pwm_base_hpp

#include <driver/gpio.h>
#include <driver/ledc.h>

#include "devs/pwm/pwm.hpp"
#include "lightdesk/headunit.hpp"
#include "readings/text.hpp"

namespace ruth {
namespace lightdesk {

//
// IMPORTANT!
//
// This object is subject to race conditions when multiple tasks call:
//  1. effects (e.g. dark(), pulse())
//  2. framePrepare()
//
// As coded this object is safe for a second task to call frameUpate().
//

class PulseWidthHeadUnit : public HeadUnit, public PwmDevice {
public:
  PulseWidthHeadUnit(uint8_t num) : PwmDevice(num) {
    PulseWidthHeadUnit::configureTimer();
    updateDuty(unitMin());
    frameChanged();

    configureChannel();

    config.min = dutyMin();
    config.max = dutyMax();
    config.dim = dutyPercent(0.004);
    config.bright = dutyMax();
    config.pulse_start = dutyPercent(0.5);
    config.pulse_end = dutyPercent(0.25);

    unitNext(config.dim, true);
  }

  virtual ~PulseWidthHeadUnit() { stop(); }

public:
  static void configureTimer() {
    if (_timer_configured == false) {
      auto timer_rc = ledc_timer_config(&_ledc_timer);

      if (timer_rc == ESP_OK) {
        _timer_configured = true;
      } else {
        using TR = reading::Text;
        TR::rlog("[%s] %s", esp_err_to_name(timer_rc), __PRETTY_FUNCTION__);
      }

      PwmDevice::allOff();
    }
  }

  virtual inline void dark() { _mode = DARK; }
  virtual inline void dim() {
    unitNext(config.dim);
    _mode = DIM_INIT;
  }

  virtual inline void fixed(const float percent) {
    unitNext(unitPercent(percent));
    _mode = FIXED_INIT;
  }

  virtual inline void framePrepare() override {
    const uint32_t duty_now = duty();

    switch (_mode) {
    case IDLE:
    case FIXED_RUNNING:
    case DIM_RUNNING:
      break;

    case DARK:
      if (duty_now > unitMin()) {
        unitNext(unitMin(), true);
      }

      _mode = IDLE;
      break;

    case DIM_INIT:
      frameChanged() = true;
      _mode = DIM_RUNNING;
      break;

    case FIXED_INIT:
      frameChanged() = true;
      _mode = FIXED_RUNNING;
      break;

    case PULSE_INIT:
      // unitNext() has already been set by the call to pulse()
      frameChanged() = true;
      _mode = PULSE_RUNNING;
      break;

    case PULSE_RUNNING:
      const int_fast32_t fuzzy = _dest + _velocity;
      const int_fast32_t next = duty_now - _velocity;

      // we've reached or are close enough to the destination
      if ((duty_now <= fuzzy) || (next <= _dest)) {
        unitNext(_dest, true);
        _mode = IDLE;
      } else {
        unitNext(next, true);
      }

      break;
    }
  }

  virtual inline void frameUpdate(uint8_t *frame_actual) override {
    // IMPORTANT!
    //
    // this function is called by the DMX task and therefore must never create
    // side effects to this class.

    // overide base class since we don't actually change the DMX frame
    if (frameChanged() == true) {
      updateDuty(_unit_next);
      frameChanged() = false;
    }
  }

  inline uint_fast32_t unitPercent(const float percent) {
    return dutyPercent(percent);
  }

  inline void pulse(float intensity = 1.0, float secs = 0.2) {
    // intensity is the percentage of max brightness for the pulse
    const float start = config.pulse_start * intensity;

    unitNext(start);
    _dest = config.pulse_end;

    // compute change per frame required to reach dark within requested secs
    _velocity = (start - _dest) / (fps() * secs);

    _mode = PULSE_INIT;
  }

  static void preStart() {
    configureTimer();
    allOff();
  }

protected:
  struct {
    uint_fast32_t min;
    uint_fast32_t max;
    uint_fast32_t dim;
    uint_fast32_t bright;
    uint_fast32_t pulse_start;
    uint_fast32_t pulse_end;
  } config;

protected:
  virtual inline void unitNext(uint_fast32_t duty, bool update = false) {
    if (duty > unitMax()) {
      duty = unitMax();
    } else if (duty < unitMin()) {
      duty = unitMin();
    }

    _unit_next = duty;
    frameChanged() = update;
  }

  uint_fast32_t &unitMax() { return config.max; }
  uint_fast32_t &unitMin() { return config.min; }

private:
  typedef enum {
    DARK = 0,
    DIM_INIT,
    DIM_RUNNING,
    IDLE,
    FIXED_INIT,
    FIXED_RUNNING,
    PULSE_INIT,
    PULSE_RUNNING
  } PulseWidthHeadUnit_t;

private:
  // class members, defined and initialized in misc/statics.cpp
  static bool _timer_configured;
  static const ledc_timer_config_t _ledc_timer;

  PulseWidthHeadUnit_t _mode = IDLE;

  uint_fast32_t _dest = 0; // destination duty when traveling
  float _velocity = 0.0;   // change per frame when fx is active

  uint_fast32_t _unit_next = 0;
};

} // namespace lightdesk
} // namespace ruth

#endif
