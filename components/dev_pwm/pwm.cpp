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

#include "dev_pwm/cmd_fixed.hpp"
#include "dev_pwm/cmd_random.hpp"
#include "dev_pwm/pwm.hpp"

namespace device {

// construct a new PulseWidth with a known address
PulseWidth::PulseWidth(uint8_t pin_num) : pwm::Hardware(pin_num) { makeStatus(); }

IRAM_ATTR bool PulseWidth::execute(const char *cmd) {
  auto rc = true;

  // simple on/off commands
  if (cmd[0] == 'o') {
    uint32_t duty = 0;

    if ((cmd[1] == 'n') && (cmd[2] == 0x00)) {
      duty = dutyMax();
    } else if ((cmd[1] == 'f') && (cmd[2] == 'f') && (cmd[3] == 0x00)) {
      duty = dutyMin();
    } else {
      rc = false;
    }

    if (rc) {
      if (_cmd) _cmd.reset(nullptr);

      updateDuty(duty);
    }
  }

  return rc;
}

IRAM_ATTR bool PulseWidth::execute(const JsonObject &root) {
  auto rc = false;
  const char *cmd = root["cmd"];
  const char *type = root["params"]["type"];

  if (type) {
    rc = false;

    if (strcmp(type, "fixed") == 0) {
      _cmd.reset(new pwm::Fixed(self(), root));
      rc = _cmd->run();
    } else if (strcmp(type, "random") == 0) {
      _cmd.reset(new pwm::Random(self(), root));
      rc = _cmd->run();
    }
  } else {
    rc = execute(cmd);
  }

  return rc;
}

IRAM_ATTR void PulseWidth::makeStatus() {
  constexpr size_t capacity = sizeof(_status) - 1;
  auto *p = _status;

  // if a cmd is running, use it's name as the status
  if (_cmd) {
    memccpy(p, _cmd->name(), 0x00, capacity);
    return;
  }

  // otherwise, create a text representation of the duty value
  uint32_t duty_now = duty();

  // going for ultimate efficiency here
  if (duty_now == dutyMin()) { // handle off
    *p++ = 'o';
    *p++ = 'f';
    *p++ = 'f';
    *p++ = 0x00;
    return;
  }

  if (duty_now == dutyMax()) { // handle on
    *p++ = 'o';
    *p++ = 'n';
    *p++ = 0x00;
    return;
  }

  // not on or off, create a custom status using the duty val
  *p++ = 'f';
  *p++ = 'i';
  *p++ = 'x';
  *p++ = 'e';
  *p++ = 'd';
  *p++ = ' ';

  itoa(duty_now, p, 10);
}
} // namespace device
