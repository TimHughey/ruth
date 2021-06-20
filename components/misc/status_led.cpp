/*
    misc/status_led.cpp - Ruth Status LED Support
    Copyright (C) 2017  Tim Hughey

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

#include <esp_attr.h>

#include "misc/status_led.hpp"

namespace ruth {

static StatusLED *status_led = nullptr;
static constexpr uint32_t duty_max = 0b1111111111111; // 13 bits, 8191
static constexpr uint32_t duty_min = 0x00;

StatusLED::StatusLED() : PulseWidth(0), _duty(duty_max / 100) {}

void StatusLED::init() {
  if (status_led) return;

  status_led = new StatusLED();
}

IRAM_ATTR void StatusLED::bright() {
  auto &duty = status_led->_duty;

  duty = duty_max / 2;
  status_led->updateDuty(duty);
}

IRAM_ATTR void StatusLED::brighter() {
  auto &duty = status_led->_duty;

  duty += 1024;

  if (duty > duty_max) {
    duty = duty_max;
  }

  status_led->updateDuty(duty);
}

IRAM_ATTR void StatusLED::dim() {
  auto &duty = status_led->_duty;

  duty = duty_max / 90;
  status_led->updateDuty(duty);
}

IRAM_ATTR void StatusLED::dimmer() {
  auto &duty = status_led->_duty;

  if (duty > 1025) {
    duty -= 1025;
  }

  status_led->updateDuty(duty);
}

IRAM_ATTR void StatusLED::off() {
  auto &duty = status_led->_duty;

  duty = 0;
  status_led->updateDuty(duty);
}

IRAM_ATTR void StatusLED::percent(float p) {
  uint32_t val = duty_max * p;

  auto &duty = status_led->_duty;

  duty = val;
  status_led->updateDuty(duty);
}

} // namespace ruth
