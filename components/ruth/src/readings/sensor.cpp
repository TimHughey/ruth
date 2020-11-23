/*
    celsius.cpp - Ruth Sensor Reading
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

#include <sys/time.h>
#include <time.h>

#include "readings/sensor.hpp"

namespace ruth {
namespace reading {

Sensor::Sensor(const char *id, float celsius) : Reading(id, SENSOR) {

  captureData(celsius);
};

Sensor::Sensor(const char *id, float celsius, float relhum)
    : Reading(id, SENSOR) {

  captureData(celsius, relhum);
};

Sensor::Sensor(const char *id, float celsius, int capacitance)
    : Reading(id, SENSOR) {

  captureData(celsius, capacitance);
};

void Sensor::populateMessage(JsonDocument &doc) {

  if (_hasValue[TEMP]) {
    doc["temp_c"] = _celsius;
    doc["temp_f"] = _fahrenheit;
  }

  if (_hasValue[RH]) {
    doc["relhum"] = _relhum;
  }

  if (_hasValue[CAPACITANCE]) {
    doc["capacitance"] = _capacitance;
  }
};

//
// Private
//

void Sensor::captureData(float celsius) {
  _celsius = celsius;
  _fahrenheit = (float)_celsius * 1.8 + 32.0;
  _hasValue[TEMP] = true;
}

void Sensor::captureData(float celsius, float relhum) {
  captureData(celsius);
  _relhum = relhum;
  _hasValue[RH] = true;
}

void Sensor::captureData(float celsius, int capacitance) {
  captureData(celsius);
  _capacitance = capacitance;
  _hasValue[CAPACITANCE] = true;
  ;
}

} // namespace reading
} // namespace ruth
