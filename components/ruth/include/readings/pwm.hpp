/*
    pwm.hpp - Ruth PWM Reading
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

#ifndef _ruth_reading_pwm_hpp
#define _ruth_reading_pwm_hpp

#include <sys/time.h>
#include <time.h>

#include "readings/reading.hpp"

namespace ruth {
namespace reading {
typedef class Pwm Pwm_t;

class Pwm : public Reading {
private:
  // actual reading data
  uint32_t duty_max_ = 4095;
  uint32_t duty_min_ = 1;
  uint32_t duty_ = 0;

public:
  Pwm(const char *id, uint32_t duty_max, uint32_t duty_min, uint32_t duty);

protected:
  virtual void populateMessage(JsonDocument &doc);
};
} // namespace reading
} // namespace ruth
#endif // __cplusplus
