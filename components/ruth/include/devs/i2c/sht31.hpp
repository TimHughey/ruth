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

#include <memory>
#include <string>

#include "devs/i2c/dev.hpp"

namespace ruth {

typedef class SHT31 SHT31_t;

class SHT31 : public I2cDevice {
public:
  SHT31();

  bool detectOnBus();
  bool read();

private:
  bool crc(const uint8_t *data);
};

} // namespace ruth

#endif
