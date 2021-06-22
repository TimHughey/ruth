/*
    Ruth
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

#include <cstring>

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_attr.h>
#include <esp_log.h>
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>

// #include "dev_pwm/cmd_random.hpp"
#include "dev_pwm/pwm.hpp"

namespace device {

// construct a new PulseWidth with a known address and compute the id
PulseWidth::PulseWidth(uint8_t pin_num) : pwm::Hardware(pin_num) {
  _status[0] = 'o';
  _status[1] = 'f';
  _status[2] = 'f';
}

void PulseWidth::makeStatus() {
  constexpr size_t capacity = sizeof(_status) - 1;

  // if a cmd is running, use it's name as the status
  if (_cmd) {
    memccpy(_status, _cmd->name(), 0x00, capacity);
    return;
  }

  uint32_t duty_now = duty();

  if (duty_now == dutyMin()) { // handle off
    _status[0] = 'o';
    _status[1] = 'f';
    _status[2] = 'f';
    _status[3] = 0x00;
    return;
  }

  if (duty_now == dutyMax()) { // handle on
    _status[0] = 'o';
    _status[1] = 'n';
    _status[3] = 0x00;
    return;
  }

  // not on or off, create a custom status using the duty
  auto *p = _status;
  *p++ = 'f';
  *p++ = 'i';
  *p++ = 'x';
  *p++ = 'e';
  *p++ = 'd';
  *p++ = ' ';

  itoa(duty_now, p, 10);
}
} // namespace device
