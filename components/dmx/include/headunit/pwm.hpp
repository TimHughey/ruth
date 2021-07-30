/*
    Ruth
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

#ifndef dmx_headunits_pwm_hpp
#define dmx_headunits_pwm_hpp

#include <driver/gpio.h>
#include <driver/ledc.h>

#include "dev_pwm/pwm.hpp"
#include "headunit.hpp"

namespace dmx {

class PulseWidthHeadUnit : public HeadUnit, public pwm::Hardware {
public:
  PulseWidthHeadUnit(uint8_t num) : Hardware(num) {}
  virtual ~PulseWidthHeadUnit() = default;

public:
  virtual void dark() override { updateDuty(0); }
  virtual void handleMsg(const JsonObject &obj) override = 0;

private:
  // class members, defined and initialized in misc/statics.cpp

  uint8_t _num;
};

} // namespace dmx

#endif
