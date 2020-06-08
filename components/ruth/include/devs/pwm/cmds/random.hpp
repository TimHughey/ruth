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

#ifndef _ruth_pwm_cmd_random_hpp
#define _ruth_pwm_cmd_random_hpp

#include <memory>
#include <string>

#include "external/ArduinoJson.h"
#include "local/types.hpp"

#include "devs/pwm/cmds/cmd.hpp"

namespace ruth {
namespace pwm {

typedef class Random Random_t;

class Random : public Command {
public:
  Random(const char *pin, ledc_channel_config_t *chan, JsonObject &cmd);
  ~Random();

protected:
  static void loop(void *task_data) {
    Random_t *obj = (Random_t *)task_data;

    obj->_loop();
  }

private:
  void _loop();

  uint32_t directionFromVal(uint32_t val) const { return (val == 0) ? -1 : 1; }
  esp_err_t setDuty(uint32_t duty);
  uint32_t random(uint32_t modulo) const { return esp_random() % modulo; }
  uint32_t randomDirection() const { return directionFromVal(random(2)); }
  uint32_t randomPrime() const { return _primes[random(_num_primes)]; }

private:
  uint32_t _max = 8191;
  uint32_t _min = 0;
  uint32_t _num_primes = 10;
  uint32_t _step = 100;
  uint32_t _step_ms = 75;

  // primes
  static uint32_t _primes[26];
};

} // namespace pwm
} // namespace ruth

#endif
