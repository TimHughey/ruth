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
uint32_t Random::_primes[36] = {2,   3,   5,   7,   11,  13,  17,  19,  23,
                                29,  31,  37,  41,  43,  47,  53,  59,  61,
                                67,  71,  73,  79,  83,  89,  97,  101, 103,
                                107, 109, 113, 127, 131, 137, 139, 149, 151};

Random::Random(const char *pin, ledc_channel_config_t *chan, JsonObject &cmd)
    : Command(pin, chan, cmd) {

  JsonObject obj = cmd["random"];

  if (obj) {
    // max duty value permitted
    _max = obj["max"] | _max;
    _min = obj["min"] | _min;

    uint32_t num_primes = obj["primes"] | _num_primes;

    // prevent the requested primes from exceeding the available primes
    _num_primes = (num_primes > availablePrimes()) ? _num_primes : num_primes;

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

  Text::rlog("cmd \"%s\" started on %s handle=%p", name().c_str(), pin(),
             taskHandle());

  // pick a random starting point
  auto duty = random((_max - _min) + _min);

  fadeTo(duty);

  do {
    // pick a random initial direction and number of steps
    auto direction = randomDirection();
    auto steps = randomPrime();

    auto in_range = true;
    auto pause_ms = randomPrime() + _step_ms;

    for (auto i = 0; (i < steps) && keepRunning() && in_range; i++) {
      uint32_t next_duty = duty + (_step * direction);

      if ((next_duty >= _max) || (next_duty <= _min)) {
        pause(randomPrime() * _step_ms);
        in_range = false;
        break;
      }

      duty = next_duty;
      setDuty(next_duty);

      pause(pause_ms);
    }
  } while (keepRunning());
}

} // namespace pwm
} // namespace ruth
