/*
    pwm.cpp - Ruth PWM Reading
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

#include <string>

#include <sys/time.h>
#include <time.h>

#include "readings/pwm.hpp"

namespace ruth {

pwmReading::pwmReading(const string_t &id, time_t mtime, uint32_t duty_max,
                       uint32_t duty_min, uint32_t duty)
    : Reading(id, mtime) {
  duty_max_ = duty_max;
  duty_min_ = duty_min;
  duty_ = duty;

  _type = ReadingType_t::PWM;
};

void pwmReading::populateJSON(JsonDocument &doc) {
  doc["duty"] = duty_;
  doc["duty_max"] = duty_max_;
  doc["duty_min"] = duty_min_;
};
} // namespace ruth
