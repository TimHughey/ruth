/*
    positions.hpp - Ruth Relative Humidity Reading
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

#ifndef _ruth_reading_positions_hpp
#define _ruth_reading_positions_hpp

#include <freertos/FreeRTOS.h>
#include <sys/time.h>
#include <time.h>

#include "readings/reading.hpp"

namespace ruth {
namespace reading {
typedef class Positions Positions_t;

class Positions : public Reading {
private:
  static const uint32_t _max_pios = 16;
  // actual reading data
  uint32_t _pios = 0;
  uint32_t _states = 0x00;

public:
  // undefined reading
  Positions(const char *id, uint32_t states, uint32_t pios);
  uint32_t state() { return _states; }

protected:
  virtual void populateMessage(JsonDocument &doc);
};
} // namespace reading
} // namespace ruth

#endif // positions_reading_h
