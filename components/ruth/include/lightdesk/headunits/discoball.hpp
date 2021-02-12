/*
    lightdesk/headunits/discoball.hpp - Ruth LightDesk Headunit Disco Ball
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

#ifndef _ruth_lightdesk_headunits_discoball_hpp
#define _ruth_lightdesk_headunits_discoball_hpp

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

typedef class DiscoBall DiscoBall_t;

class DiscoBall : public PulseWidthHeadUnit {

public:
  DiscoBall(uint8_t pwm_num) : PulseWidthHeadUnit(pwm_num) {
    configureChannel();
    // ensure the el wire is dark when created
    _duty_next = dutyMin();
    frameChanged() = true;
  }

  ~DiscoBall() { stop(); }

public: // effects
  inline void dark() { _mode = STILL; }

  inline void spin() {
    _duty_next = _slow;

    _mode = SPIN;
  }

  inline void still() { _mode = STILL; }

public:
  // overides of HeadUnit base class
  inline void framePrepare() {
    const uint32_t duty_now = duty();

    switch (_mode) {
    case IDLE:
    case SPINNING:
      break;

    case STILL:
      if (duty_now > dutyMin()) {
        _duty_next = dutyMin();
        frameChanged() = true;
      }

      _mode = IDLE;
      break;

    case SPIN:
      // _duty_next has already been set by the call to spin()
      frameChanged() = true;
      _mode = SPINNING;
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
  typedef enum { STILL = 0, IDLE, SPIN, SPINNING } DiscoBallMode_t;

private:
  DiscoBallMode_t _mode = STILL;

  uint32_t _slow = 5200;

  uint32_t _duty_next = 0;
};

} // namespace lightdesk
} // namespace ruth

#endif
