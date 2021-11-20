/*
    include/pwm/cmds/fixed.hpp - Ruth PWM Fixed Command Class
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

#ifndef _ruth_pwm_cmd_fixed_hpp
#define _ruth_pwm_cmd_fixed_hpp

#include "ArduinoJson.h"
#include "dev_pwm/cmd.hpp"

namespace pwm {

class Fixed : public Command {
public:
  Fixed(Hardware *hardware, const JsonObject &cmd);
  ~Fixed();

protected:
  static void loop(void *task_data);

private:
  struct Opts {
    uint32_t duty = 0;
  };

private:
  // static size_t availablePrimes();
  //
  // static int32_t randomDirection();
  // static uint32_t randomNum(uint32_t modulo);
  // static uint32_t randomPrime(uint8_t num_primes = 0);

private:
  Opts opts;
};

} // namespace pwm

#endif
