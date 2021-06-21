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

static char base_name[32] = {};

// construct a new PulseWidth with a known address and compute the id
PulseWidth::PulseWidth(uint8_t pin_num) : pwm::Hardware(pin_num) { makeID(); }

void PulseWidth::makeID() {
  constexpr size_t capacity = sizeof(_id) / sizeof(char);
  auto remaining = capacity;
  auto *p = _id;

  // very efficiently populate the prefix
  *p++ = 'p';
  *p++ = 'w';
  *p++ = 'm';
  *p++ = '/';

  remaining -= 4;
  memccpy(p, base_name, 0x00, remaining);

  p--;        // back up one, memccpy returns pointer to byte immediately after the copied null
  *p++ = ':'; // add the divider between base name and pin identifier
  remaining -= p - _id;

  memccpy(p, shortName(), 0x00, remaining);
}

void PulseWidth::setBaseName(const char *base) {
  constexpr size_t capacity = sizeof(base_name) / sizeof(char);
  memccpy(base_name, base, 0x00, capacity);
}

} // namespace device
