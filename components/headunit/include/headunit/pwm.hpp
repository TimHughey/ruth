//  Ruth
//  Copyright (C) 2021  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "dev_pwm/pwm.hpp"
#include "headunit/headunit.hpp"
#include "ru_base/types.hpp"

#include <driver/gpio.h>
#include <driver/ledc.h>

namespace ruth {

class PulseWidthHeadUnit : public HeadUnit, public pwm::Hardware {
public:
  PulseWidthHeadUnit(csv id, uint8_t num) noexcept
      : HeadUnit(id), // set module id in HeadUnit
        Hardware(num) // set pwm num in pwm::Hardware
  {}

  virtual ~PulseWidthHeadUnit() = default;

public:
  virtual void dark() noexcept override { updateDuty(0); }
  virtual void handle_msg(JsonDocument &doc) noexcept override = 0;
};

} // namespace ruth
