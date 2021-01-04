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

#include "devs/pwm/cmds/cmd.hpp"
#include "external/ArduinoJson.h"
#include "misc/random.hpp"

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

  int32_t directionFromVal(uint32_t val) const {

    switch (val) {
    case 0:
      return 0;

    case 1:
      return -1;

    case 2:
      return 1;

    default:
      // favor decreasing brightness
      return -1;
    }
  }

  int32_t randomDirection() const { return directionFromVal(random(3)); }

private:
  uint32_t _max = 8191;
  uint32_t _min = 0;
  uint32_t _num_primes = 35;
  uint32_t _step = 7;
  uint32_t _step_ms = 65;
};

} // namespace pwm
} // namespace ruth

#endif
