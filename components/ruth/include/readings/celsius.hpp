/*
    celsius.hpp - Ruth Celsius Reading
    Copyright (C) 2017  Tim Hughey

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

#ifndef _ruth_celsius_reading_hpp
#define _ruth_celsius_reading_hpp

#include <sys/time.h>
#include <time.h>

#include "readings/reading.hpp"

namespace ruth {
typedef class celsiusReading celsiusReading_t;

class celsiusReading : public Reading {
private:
  // actual reading data
  float _celsius = 0.0;

public:
  celsiusReading(const string_t &id, float celsius);

protected:
  virtual void populateJSON(JsonDocument &doc);
};

} // namespace ruth
#endif // __cplusplus
