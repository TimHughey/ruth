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

#ifndef _ruth_pwm_dev_hpp
#define _ruth_pwm_dev_hpp

#include <memory>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ArduinoJson.h"
#include "dev_pwm/cmd.hpp"
#include "dev_pwm/hardware.hpp"

namespace device {
class PulseWidth : public pwm::Hardware {

public:
  PulseWidth(uint8_t pin_num);

  inline uint8_t devAddr() const { return pinNum(); }
  bool execute(const JsonObject &root);
  // bool execute(const char *cmd);
  bool execute(pwm::CmdWrapped cmd);
  const char *id() const { return shortName(); }

  void makeStatus();

  const char *status() const { return _status; }

private:
  enum CmdType : uint32_t { NO_MATCH = 0, ON, OFF, FIXED, RANDOM };

private:
  CmdType cmdType(const JsonObject &root) const;
  inline void cmdBasic(const CmdType type) { updateDuty(type == CmdType::ON ? dutyMax() : dutyMin()); }

private:
  char _status[32];
  pwm::CmdWrapped _cmd;
};
} // namespace device

#endif // pwm_dev_hpp
