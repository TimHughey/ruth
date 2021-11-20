/*
    dev_pwm/cmds/basic.cpp
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

#include "dev_pwm/cmd_random.hpp"

namespace pwm {

static uint32_t _primes[] = {2,   3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,  47,
                             53,  59,  61,  67,  71,  73,  79,  83,  89,  97,  101, 103, 107, 109, 113,
                             127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197,
                             199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277};

Random::Random(Hardware *hardware, const JsonObject &cmd) : Command(hardware, cmd) {

  JsonObject params = cmd["params"];

  if (params) {
    // max duty value permitted
    opts.max = params["max"] | opts.max;
    opts.min = params["min"] | opts.min;

    uint32_t num_primes = params["primes"] | opts.num_primes;

    // prevent the requested primes from exceeding the available primes
    opts.num_primes = (num_primes > (availablePrimes() - 1)) ? opts.num_primes : num_primes;

    opts.step = params["step"] | opts.step;
    opts.step_ms = params["step_ms"] | opts.step_ms;
  }

  loopData(this);
  loopFunction(&loop);
}

Random::~Random() {
  kill(); // kill our process, if running, before memory is released
}

IRAM_ATTR void Random::loop(void *task_data) {
  Random *obj = (Random *)task_data;

  // pick a random starting point
  const auto duty_max = obj->opts.max;
  const auto duty_min = obj->opts.min;
  const auto step_ms = obj->opts.step_ms;
  auto duty = randomNum((duty_max - duty_min) + duty_min);

  obj->fadeTo(duty);

  do {
    // pick a random initial direction and number of steps
    auto direction = randomDirection();
    auto steps = randomPrime();

    auto in_range = true;
    auto pause_ms = randomPrime() + step_ms;

    for (uint32_t i = 0; (i < steps) && obj->keepRunning() && in_range; i++) {
      uint32_t next_duty = duty + (obj->opts.step * direction);

      if ((next_duty >= duty_max) || (next_duty <= duty_min)) {
        obj->pause(randomPrime() * step_ms);
        in_range = false;
        break;
      }

      duty = next_duty;
      obj->_hw->updateDuty(next_duty);

      obj->pause(pause_ms);
    }
  } while (obj->keepRunning());
}

IRAM_ATTR uint32_t Random::availablePrimes() {
  constexpr size_t num_primes = (sizeof(_primes) / sizeof(uint32_t)) - 1;

  return num_primes;
}

IRAM_ATTR int32_t Random::randomDirection() {
  static const int32_t direction[] = {0, -1, 1};

  return direction[randomNum(3)];
}

IRAM_ATTR uint32_t Random::randomNum(uint32_t modulo) { return (esp_random() % modulo) + 1; }

IRAM_ATTR uint32_t Random::randomPrime(uint8_t num_primes) {
  uint8_t index; // reminder, next must be >=0 and < availablePrimes()

  if ((num_primes == 0) || (num_primes > availablePrimes())) {
    index = randomNum(availablePrimes()) - 1;
  } else {
    index = randomNum(num_primes) - 1;
  }

  return _primes[index];
}

} // namespace pwm
