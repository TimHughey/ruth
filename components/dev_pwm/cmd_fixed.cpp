/*
    dev_pwm/cmds/cme_fixed.cpp
    Ruth PWM Fixed Command Class Implementation

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

#include <algorithm>

#include <esp_log.h>

#include "dev_pwm/cmd_fixed.hpp"

namespace pwm {

Fixed::Fixed(Hardware *hardware, const JsonObject &cmd) : Command(hardware, cmd) {

  JsonObject params = cmd["params"];

  if (params) {
    // requested percent
    auto percent = params["percent"].as<float>();
    percent = (percent <= 100.0f) ? percent : 100.0f;

    opts.duty = hardware->dutyPercent(percent);
  }

  loopData(this);
  loopFunction(&loop);
}

Fixed::~Fixed() {
  kill(); // kill our process, if running, before memory is released
}

IRAM_ATTR void Fixed::loop(void *task_data) {
  Fixed *obj = (Fixed *)task_data;

  do {
    obj->setDuty(obj->opts.duty);

    obj->pause(1000);

  } while (obj->keepRunning());
}

} // namespace pwm
