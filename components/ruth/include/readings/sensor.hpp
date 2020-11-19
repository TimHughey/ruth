/*
    celsius.hpp - Ruth Sensormental Reading
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

#ifndef _ruth_reading_sensor_hpp
#define _ruth_reading_sensor_hpp

#include <sys/time.h>
#include <time.h>

#include "readings/reading.hpp"

namespace ruth {
namespace reading {
typedef class Sensor Sensor_t;

class Sensor : public Reading {

  typedef enum { TEMP = 0, RH, CAPACITANCE, END_OF_VAL_LIST } SensorValues_t;

private:
  // data read directly from the sensor
  float _celsius = 0.0;
  float _relhum = 0.0;
  int _capacitance = 0;

  // calculated senor data
  float _fahrenheit = 0;

  // list of booleans that indicate if a specific data point will be
  // sent as part of the payload
  bool _hasValue[3] = {};

public:
  Sensor(const char *id, float celsius);
  Sensor(const char *id, float celsius, float rel_hum);
  Sensor(const char *id, float celsius, int capacitance);

protected:
  void populateJSON(JsonDocument &doc);

private:
  void captureData(float celsius); // temperature only sensors
  void captureData(float celsius, float relhum);
  void captureData(float celsius, int capacitance);
};
} // namespace reading
} // namespace ruth
#endif
