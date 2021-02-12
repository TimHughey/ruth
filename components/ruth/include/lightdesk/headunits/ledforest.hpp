/*
    lightdesk/headunits/ledforest.hpp - Ruth LightDesk Headunit LED Forest
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

#ifndef _ruth_lightdesk_headunits_ledforest_hpp
#define _ruth_lightdesk_headunits_ledforest_hpp

#include "lightdesk/headunits/pwm.hpp"

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

typedef class LedForest LedForest_t;

class LedForest : public PulseWidthHeadUnit {

public:
  LedForest(uint8_t pwm_num) : PulseWidthHeadUnit(pwm_num) {
    configureChannel();
    // ensure the el wire is dark when created
    frameChanged() = true;
  }

  ~LedForest() { stop(32); }

public: // effects
  inline void dark() { _mode = DARK; }

  inline void pulse(float intensity = 1.0, float secs = 0.7) {
    // intensity is the percentage of max brightness for the pulse
    float start = _pulse_bright * intensity;
    _dest = start;
    _duty_next = start;

    // compute change per frame required to reach dark within requested secs
    _velocity = (start - _dark) / (fps() * secs);

    _mode = PULSE_INIT;
  }

public:
  // overides of HeadUnit base class
  inline void framePrepare() {
    const uint32_t duty_now = duty();

    switch (_mode) {
    case IDLE:
      break;

    case DARK:
      if (duty_now > _dark) {
        _duty_next = _dark;
        frameChanged() = true;
      }

      _mode = IDLE;
      break;

    case PULSE_INIT:
      // _duty_next has already been set by the call to pulse()
      frameChanged() = true;
      _mode = PULSE_RUNNING;
      break;

    case PULSE_RUNNING:
      const int_fast32_t fuzzy = _dest + _velocity;
      const int_fast32_t next = duty_now - _velocity;

      // we've reached or are close enough to the destination
      if ((duty_now <= fuzzy) || (next <= _dest)) {
        _duty_next = _dest;
        frameChanged() = true;
        _mode = IDLE;
      } else {
        _duty_next = next;
        frameChanged() = true;
      }

      break;
    }
  }

  inline void frameUpdate(uint8_t *frame_actual) {
    // IMPORTANT!
    //
    // this function is called by the DMX task and therefore must never create
    // side effects to this class.

    // overide base class since we don't actually change the DMX frame
    if (frameChanged() == true) {
      updateDuty(_duty_next);
      frameChanged() = false;
    }
  }

private:
  typedef enum { DARK = 0, IDLE, PULSE_INIT, PULSE_RUNNING } LedForestMode_t;

private:
  LedForestMode_t _mode = DARK;

  uint_fast32_t _dest = 0; // destination duty when traveling
  float _velocity = 0.0;   // change per frame when fx is active

  uint32_t _pulse_bright = 1024.0;
  uint32_t _bright = dutyMax();
  uint32_t _dark = dutyMin();

  uint32_t _duty_next = 0;
};

} // namespace lightdesk
} // namespace ruth

#endif
