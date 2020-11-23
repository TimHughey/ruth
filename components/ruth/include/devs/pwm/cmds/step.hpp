/*
    include/pwm/cmds/step.hpp - Ruth PWM Sequence Step Class
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

#ifndef _ruth_pwm_cmd_step_hpp
#define _ruth_pwm_cmd_step_hpp

#include "external/ArduinoJson.h"
#include "local/types.hpp"

namespace ruth {
namespace pwm {

typedef class Step Step_t;

class Step {
public:
  // must always create a Step with actual values
  Step() = delete;

  // create a Step from base types
  Step(uint32_t duty, uint32_t ms) : _duty(duty), _ms(ms){};

  // create a Step from a JsonObject
  Step(JsonObject obj) {
    _duty = obj["duty"] | 0;
    _ms = obj["ms"] | 0;
  }

  // copy constructor
  Step(const Step &s) : _duty(s._duty), _ms(s._ms){};

  // member access
  uint32_t duty() const { return _duty; }
  uint32_t ms() const { return _ms; }

private:
  uint32_t _duty;
  uint32_t _ms;
};
} // namespace pwm

} // namespace ruth

#endif
