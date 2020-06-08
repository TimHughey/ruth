/*
    i2c_dev.cpp - Ruth I2C Device
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

#include <memory>
#include <string>

#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <sys/time.h>
#include <time.h>

#include "devs/base/base.hpp"
#include "devs/i2c/dev.hpp"
#include "local/types.hpp"
#include "net/network.hpp"

using std::unique_ptr;

namespace ruth {

const char *I2cDevice::I2cDeviceDesc(uint8_t addr) {
  switch (addr) {
  case 0x44:
    return (const char *)"sht31";
    break;

  case 0x20:
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x24:
  case 0x25:
  case 0x26:
  case 0x27:
    return (const char *)"mcp23008";
    break;

  case 0x36:
    return (const char *)"soil";
    break;

  default:
    return (const char *)"unknown";
    break;
  }
}

// construct a new I2cDevice with a known address and compute the id
I2cDevice::I2cDevice(DeviceAddress_t &addr, bool use_multiplexer, uint8_t bus)
    : Device(addr) {
  _use_multiplexer = use_multiplexer;
  _bus = bus;

  auto const max_id_len = 63;
  unique_ptr<char[]> id(new char[max_id_len + 1]);

  setDescription(I2cDeviceDesc(firstAddressByte()));

  snprintf(id.get(), max_id_len, "i2c/%s.%02x.%s", Net::hostname(), this->bus(),
           description().c_str());

  setID(id.get());

  _raw_data.reserve(24);
};

uint8_t I2cDevice::devAddr() { return firstAddressByte(); };
bool I2cDevice::useMultiplexer() { return _use_multiplexer; };
uint8_t I2cDevice::bus() const { return _bus; };

const RawData_t &I2cDevice::rawData() { return _raw_data; }

uint8_t I2cDevice::readAddr() {
  return (firstAddressByte() << 1) | I2C_MASTER_READ;
};

void I2cDevice::storeRawData(RawData_t &data) { _raw_data = data; }

uint8_t I2cDevice::writeAddr() {
  return (firstAddressByte() << 1) | I2C_MASTER_WRITE;
};

const unique_ptr<char[]> I2cDevice::debug() {
  const auto max_len = 127;
  unique_ptr<char[]> debug_str(new char[max_len + 1]);

  snprintf(debug_str.get(), max_len, "I2cDevice(%s bus=%d use_mplex=%s)",
           id().c_str(), _bus, (_use_multiplexer) ? "true" : "false");

  return move(debug_str);
}
} // namespace ruth
