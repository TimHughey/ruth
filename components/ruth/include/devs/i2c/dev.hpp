/*
    i2c_dev.hpp - Ruth I2C
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

#ifndef _ruth_i2c_dev_hpp
#define _ruth_i2c_dev_hpp

#include <memory>
#include <string>

#include "devs/base/base.hpp"

namespace ruth {

using std::unique_ptr;

typedef class I2cDevice I2cDevice_t;
typedef class std::vector<uint8_t> RawData_t;

class I2cDevice : public Device {
public:
  I2cDevice() {}
  static const char *I2cDeviceDesc(uint8_t addr);

private:
  static const uint32_t _i2c_max_addr_len = 1;
  static const uint32_t _i2c_max_id_len = 30;
  static const uint32_t _i2c_addr_byte = 0;
  bool _use_multiplexer = false; // is the multiplexer is needed to reach device
  uint8_t _bus = 0; // if using multiplexer then this is the bus number
                    // where the device is hosted
  RawData_t _raw_data;

public:
  // construct a new I2cDevice with a known address and compute the id
  I2cDevice(DeviceAddress_t &addr, bool use_multiplexer = false,
            uint8_t bus = 0);

  uint8_t devAddr();
  bool useMultiplexer();
  uint8_t bus() const;
  const RawData_t &rawData();
  uint8_t readAddr();
  void storeRawData(RawData_t &data);
  uint8_t writeAddr();

  // info / debug functions
  const unique_ptr<char[]> debug();
};
} // namespace ruth

#endif // i2c_dev_h
