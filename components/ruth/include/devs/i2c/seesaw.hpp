/*
    i2c/seesaw.hpp - Ruth I2C Device Seesaw
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

#ifndef _ruth_i2c_dev_seesaw_hpp
#define _ruth_i2c_dev_seesaw_hpp

#include "devs/i2c/base.hpp"

namespace ruth {

typedef class Seesaw Seesaw_t;

class Seesaw : public I2cDevice {
public:
  Seesaw(uint8_t bus = 0, uint8_t addr = 0x36);
  Seesaw(const Seesaw_t &rhs, time_t missing_secs);

  bool detect();
  bool read();
  bool writeState(uint32_t cmd_mask, uint32_t cmd_state);

private:
  bool crc(const uint8_t *data);

  RawData_t _tx;
  RawData_t _rx;
};

} // namespace ruth

#endif
