/*
    include/pwm/cmds/random.hpp - Ruth PWM Random Sequence Class
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

#include "ArduinoJson.h"
#include "dev_pwm/cmd.hpp"

namespace ruth {
namespace pwm {

class Random : public Command {
public:
  Random(Hardware *hardware, const JsonObject &cmd);
  ~Random();

protected:
  static void loop(void *task_data);

private:
  struct Opts {
    uint32_t max = 8191;
    uint32_t min = 0;
    uint32_t num_primes = 35;
    uint32_t step = 7;
    uint32_t step_ms = 65;
  };

private:
  static uint32_t availablePrimes();

  static int32_t randomDirection();
  static uint32_t randomNum(uint32_t modulo);
  static uint32_t randomPrime(uint8_t num_primes = 0);

private:
  Opts opts;
};

} // namespace pwm
} // namespace ruth
