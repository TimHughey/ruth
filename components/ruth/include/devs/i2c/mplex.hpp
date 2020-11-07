/*
    mplex.hpp - Ruth I2C Multiplexer Device
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

#ifndef _ruth_i2c_dev_mplex_hpp
#define _ruth_i2c_dev_mplex_hpp

#include <memory>
#include <string>

#include "devs/i2c/base.hpp"

namespace ruth {

typedef class TCA9548B TCA9548B_t;
typedef TCA9548B_t I2cMultiplexer_t;

class TCA9548B : public I2cDevice {
public:
  TCA9548B();

  bool detect();
  uint32_t maxBuses() const { return _max_buses; }
  // reads of the multiplexer device are not required

  bool selectBus(uint8_t bus);

  const char *description() const { return (const char *)"TCA9548B"; };

private:
  static const uint32_t _max_buses = 8;

  uint32_t _bus_selects = 0;
  uint32_t _bus_select_errors = 0;

  RawData_t _tx;
  RawData_t _rx;
};

} // namespace ruth

#endif
