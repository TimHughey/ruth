/*
    i2c/base.hpp - Ruth I2C Device Base
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

#ifndef _ruth_i2c_device_base_hpp
#define _ruth_i2c_device_base_hpp

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
  uint8_t _bus = 0; // with multiplexer 0 >= x <= 8, zero otherwise
  RawData_t _raw_data;
  RawData_t _buffer;

  esp_err_t _esp_rc_prev = ESP_OK;
  esp_err_t _esp_rc = ESP_OK;

public:
  // construct a new I2cDevice with a known address and compute the id
  I2cDevice(const DeviceAddress_t &addr, uint8_t bus = 0);
  // I2cDevice(uint8_t addr, uint8_t bus = 0);

  uint8_t devAddr();
  uint8_t bus() const;

  void constructCommon();
  void makeID();

  const RawData_t &rawData();
  uint8_t readAddr();
  void storeRawData(RawData_t &data);
  static int timeoutDefault();
  uint8_t writeAddr();

  // info / debug functions
  const unique_ptr<char[]> debug();

protected:
  RawData_t _tx;
  RawData_t _rx;

  void clearPreviousError() { _esp_rc_prev = ESP_OK; };
  bool hasPreviousErrpr() { return (_esp_rc_prev == ESP_OK) ? false : true; };
  esp_err_t previousError() { return _esp_rc_prev; };

  bool requestData();
};
} // namespace ruth

#endif // i2c_dev_h
