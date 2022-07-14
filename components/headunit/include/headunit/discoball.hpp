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

#pragma once

#include "pwm.hpp"

namespace ruth {

class DiscoBall : public PulseWidthHeadUnit {

public:
  DiscoBall(csv id, uint8_t pwm_num) : PulseWidthHeadUnit(id, pwm_num) {}

  void handleMsg(JsonObjectConst obj) override {
    const uint32_t duty = obj[moduleId()] | 0;

    updateDuty(duty);
  }
};

} // namespace ruth
