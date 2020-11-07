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
#include "devs/i2c/base.hpp"
#include "local/types.hpp"
#include "net/network.hpp"

using std::move;
using std::unique_ptr;

namespace ruth {

static int _timeout_default = 0;

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

I2cDevice::I2cDevice(const DeviceAddress_t &addr, uint8_t bus,
                     time_t missing_secs)
    : Device(addr), _bus(bus) {
  // initialize the class constant default timeout, if needed
  if (_timeout_default == 0) {
    i2c_get_timeout(I2C_NUM_0, &_timeout_default);
  }

  setDescription(I2cDeviceDesc(singleByteAddress()));
  setMissingSeconds(missing_secs);
}

bool I2cDevice::operator==(const I2cDevice_t &rhs) const {
  return ((address() == rhs.address()) && (_bus == rhs._bus));
}

bool I2cDevice::busWrite(RawData_t &tx, float timeout_scale) {
  i2c_cmd_handle_t cmd = nullptr;
  // uint32_t timeout = _timeout_default * timeout_scale;

  if (_esp_rc_prev != ESP_OK) {
    return false;
  }

  cmd = i2c_cmd_link_create(); // allocate i2c cmd queue
  i2c_master_start(cmd);       // queue i2c START

  // queue the device address (with WRITE) and check for ACK
  i2c_master_write_byte(cmd, writeAddr(), true);

  // queue bytes to send (with ACK check)
  i2c_master_write(cmd, tx.data(), tx.size(), true);
  i2c_master_stop(cmd); // queue i2c STOP

  // execute queued i2c cmd
  _esp_rc = i2c_master_cmd_begin(I2C_NUM_0, cmd, _cmd_timeout);
  i2c_cmd_link_delete(cmd);

  if (_esp_rc != ESP_OK) {
    writeFailure();
    _esp_rc_prev = _esp_rc;

    return false;
  }

  return true;
}

bool I2cDevice::checkForOk() {
  if (_esp_rc != ESP_OK) {
    readFailure();
    checkForTimeout();

    return false;
  }
  return true;
}

void I2cDevice::checkForTimeout() {
  if (_esp_rc == ESP_ERR_TIMEOUT) {
    _timeouts++;

    if (_timeouts > _timeouts_max) {
      Text::rlog("device \"%s\" timeout, total[%ld]", id().c_str(), _timeouts);
    }
  }
}

void I2cDevice::makeID() {
  vector<char> buffer;

  buffer.reserve(maxIdLen());

  auto length = snprintf(buffer.data(), buffer.capacity(), "i2c/%s.%02x.%s",
                         Net::hostname(), this->bus(), description());

  const string_t id(buffer.data(), length);

  setID(move(id));
}

// placed in .cpp file due to required esp i2c definitions
// (e.g. I2C_MASTER_READ)
uint8_t I2cDevice::readAddr() const {
  return (singleByteAddress() << 1) | I2C_MASTER_READ;
};

bool I2cDevice::requestData(RawData_t &tx, RawData_t &rx, float timeout_scale) {
  i2c_cmd_handle_t cmd = nullptr;
  int timeout = _timeout_default * timeout_scale;

  if (_esp_rc_prev != ESP_OK) {
    return false;
  }

  readStart();

  if (timeout != timeoutDefault()) {
    i2c_set_timeout(I2C_NUM_0, timeout);
  }

  cmd = i2c_cmd_link_create(); // allocate i2c cmd queue
  i2c_master_start(cmd);       // queue i2c START

  // if there are bytes to TX queue them
  if (tx.size() > 0) {
    // queue the WRITE for device and check for ACK
    i2c_master_write_byte(cmd, writeAddr(), true);

    // queue the device command bytes
    i2c_master_write(cmd, tx.data(), tx.size(), I2C_MASTER_ACK);
  }

  // clock stretching is leveraged in the event the device requires time
  // to execute the command (e.g. temperature conversion)
  // use timeout scale to adjust time to wait for clock, if needed
  if (rx.size() > 0) {
    // start a new command sequence without sending a stop
    i2c_master_start(cmd);

    // queue the READ for device and check for ACK
    i2c_master_write_byte(cmd, readAddr(), true);

    // queue the READ of number of bytes
    i2c_master_read(cmd, rx.data(), rx.size(), I2C_MASTER_LAST_NACK);
  }

  // always queue the stop command
  i2c_master_stop(cmd);

  // execute queued i2c cmd
  _esp_rc = i2c_master_cmd_begin(I2C_NUM_0, cmd, _cmd_timeout);
  i2c_cmd_link_delete(cmd);

  // if the timeout was changed restore it
  if (timeout != timeoutDefault()) {
    i2c_set_timeout(I2C_NUM_0, timeoutDefault());
  }

  _esp_rc_prev = _esp_rc;

  readStop();
  return checkForOk();
}

// STATIC
int I2cDevice::timeoutDefault() { return _timeout_default; }

uint8_t I2cDevice::writeAddr() const {
  return (singleByteAddress() << 1) | I2C_MASTER_WRITE;
};

const unique_ptr<char[]> I2cDevice::debug() {
  const auto max_len = 127;
  unique_ptr<char[]> debug_str(new char[max_len + 1]);

  snprintf(debug_str.get(), max_len, "I2cDevice(%s bus=%d)", id().c_str(),
           _bus);

  return move(debug_str);
}
} // namespace ruth
