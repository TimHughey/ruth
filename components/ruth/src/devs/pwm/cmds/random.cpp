/*
    devs/pwm/cmds/basic.cpp
    Ruth PWM Random Command Class Implementation

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

#include <algorithm>

#include <esp_log.h>

#include "devs/pwm/cmds/random.hpp"
#include "readings/text.hpp"

namespace ruth {
namespace pwm {

// initialize the array of primes
uint32_t Random::_primes[35] = {2,   3,   5,   7,   11,  13,  17,  19,  23,
                                29,  31,  37,  41,  43,  47,  53,  59,  61,
                                67,  71,  73,  79,  83,  89,  97,  101, 103,
                                107, 109, 113, 127, 131, 137, 139, 149};

Random::Random(const char *pin, ledc_channel_config_t *chan, JsonObject &cmd)
    : Command(pin, chan, cmd) {

  JsonObject obj = cmd["random"];

  if (obj) {
    // max duty value permitted
    _max = obj["max"] | _max;
    _min = obj["min"] | _min;
    _num_primes = obj["primes"] | _num_primes;
    _step = obj["step"] | _step;
    _step_ms = obj["step_ms"] | _step_ms;
  }

  loopData(this);
  useLoopFunction(&loop);
}

Random::~Random() {
  kill(); // kill our process, if running, before freeing the steps
}

void Random::_loop() {

  // pick a random starting point
  auto curr_duty = random((_max - _min) + _min);

  do {
    // pick a random initial direction and number of steps
    auto direction = randomDirection();
    auto steps = randomPrime();

    for (auto i = 0; (i < steps) && keepRunning(); i++) {
      auto next_duty = curr_duty + (_step * direction);

      if ((next_duty >= _max) || (next_duty <= _min)) {
        pause(randomPrime() + _step_ms);
        break;
      }

      curr_duty = next_duty;
      setDuty(next_duty);

      pause(randomPrime() + _step_ms);
    }
  } while (keepRunning());
}

esp_err_t Random::setDuty(uint32_t duty) {
  const ledc_channel_config_t *chan = channel();
  const ledc_mode_t mode = chan->speed_mode;
  const ledc_channel_t channel = chan->channel;

  return ledc_set_duty_and_update(mode, channel, duty, 0);
}

} // namespace pwm
} // namespace ruth
