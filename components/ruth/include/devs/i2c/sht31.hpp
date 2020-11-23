/*
    sht31.hpp - Ruth I2C SHT31 Device
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

#ifndef _ruth_i2c_dev_sht31_hpp
#define _ruth_i2c_dev_sht31_hpp

#include "devs/i2c/base.hpp"

namespace ruth {

typedef class SHT31 SHT31_t;

class SHT31 : public I2cDevice {
public:
  SHT31(uint8_t bus = 0, uint8_t addr = 0x44);
  SHT31(const SHT31_t &rhs, time_t missing_secs = 60);

  bool detect();
  bool read();

private:
  RawData_t _tx;
  RawData_t _rx;

  bool crc(const RawData_t &data, size_t index = 0);
};

} // namespace ruth

#endif
