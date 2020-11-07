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
#include "devs/i2c/rawdata.hpp"

namespace ruth {

using std::unique_ptr;

typedef class I2cDevice I2cDevice_t;

class I2cDevice : public Device {
public:
  I2cDevice() {}
  static const char *I2cDeviceDesc(uint8_t addr);

private:
  uint8_t _bus = 0; // with multiplexer 0 >= x <= 8, zero otherwise

  esp_err_t _esp_rc_prev = ESP_OK;
  esp_err_t _esp_rc = ESP_OK;

  TickType_t _cmd_timeout = pdMS_TO_TICKS(2000);
  static const uint64_t _timeouts_max = 5;
  uint64_t _timeouts = 0;

public:
  // construct a new I2cDevice with a known address and compute the id
  I2cDevice(const DeviceAddress_t &addr, uint8_t bus = 0,
            time_t missing_secs = 600);

  bool operator==(const I2cDevice_t &rhs) const;

  uint8_t devAddr() const { return firstAddressByte(); }
  uint8_t bus() const { return _bus; }

  bool busWrite(RawData_t &tx, float timeout_scale = 1.0);

  bool checkForOk();
  void checkForTimeout();

  virtual const char *description() const {
    return I2cDeviceDesc(singleByteAddress());
  };

  virtual bool detect() { return false; };

  void makeID();

  virtual bool read() { return false; };
  virtual bool writeState(uint32_t cmd_mask, uint32_t cmd_state) {
    return false;
  }
  uint8_t readAddr() const;
  static int timeoutDefault();
  uint8_t writeAddr() const;

  // info / debug functions
  const unique_ptr<char[]> debug();

protected:
  void clearPreviousError() { _esp_rc_prev = ESP_OK; }
  bool hasPreviousErrpr() { return (_esp_rc_prev == ESP_OK) ? false : true; }
  esp_err_t previousError() { return _esp_rc_prev; }

  esp_err_t recentError() { return _esp_rc; }

  bool requestData(RawData_t &tx, RawData_t &rx, float timeout_scale = 1.0);
};
} // namespace ruth

#endif
