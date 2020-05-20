/*
    celsius.cpp - Ruth Seesaw Soil Reading
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

#include <string>

#include <sys/time.h>
#include <time.h>

#include "readings/soil.hpp"

namespace ruth {
soilReading::soilReading(const string_t &id, float celsius, int soil_moisture)
    : celsiusReading(id, celsius) {
  // override the reading type from the base class
  _type = ReadingType_t::SOIL;
  _soil_moisture = soil_moisture;
};

void soilReading::populateJSON(JsonDocument &doc) {
  // the reading is:
  //  1. capacitive reading of soil moisture
  //  2. celsius temperature of the probe

  celsiusReading::populateJSON(doc);

  doc["cap"] = _soil_moisture;
};
} // namespace ruth
