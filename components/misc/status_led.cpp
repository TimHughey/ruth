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

#include "misc/status_led.hpp"

#include <esp_attr.h>

namespace ruth {

static StatusLED *status_led = nullptr;

StatusLED::StatusLED() : PulseWidth(0) {
  auto initial_duty = dutyPercent(10);
  updateDuty(initial_duty);
}

void StatusLED::init() {
  if (status_led)
    return;

  status_led = new StatusLED();
}

IRAM_ATTR void StatusLED::bright() {
  auto duty_now = status_led->duty(nullptr);
  status_led->updateDuty(duty_now / 2);
}

IRAM_ATTR void StatusLED::brighter() {
  auto duty_now = status_led->duty(nullptr);

  status_led->updateDuty(duty_now + 1024);
}

IRAM_ATTR PulseWidth &StatusLED::device() { return *status_led; }

IRAM_ATTR void StatusLED::dim() {
  auto duty_max = status_led->dutyMax();
  status_led->updateDuty(duty_max / 90);
}

IRAM_ATTR void StatusLED::dimmer() {
  auto duty = status_led->duty(nullptr);

  if (duty > 1025)
    duty -= 1025;
  else
    duty = 0;

  status_led->updateDuty(duty);
}

IRAM_ATTR void StatusLED::off() {
  auto duty = status_led->dutyMin();

  status_led->updateDuty(duty);
}

IRAM_ATTR void StatusLED::percent(float p) {
  uint32_t duty = status_led->dutyPercent(p);

  status_led->updateDuty(duty);
}

} // namespace ruth
