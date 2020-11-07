/*
    sht31.cpp - Ruth I2C Device SHT31
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

#include <driver/i2c.h>
#include <esp_log.h>

#include "devs/i2c/sht31.hpp"

namespace ruth {

SHT31::SHT31(uint8_t bus, uint8_t addr) : I2cDevice(addr, bus) {}

SHT31::SHT31(const SHT31_t &rhs, time_t missing_secs)
    : I2cDevice(rhs.address(), rhs.bus(), missing_secs) {
  // only make the device ID when a copy is made from a device reference
  makeID();
}

// bool SHT31::crc(const uint8_t *data) {
bool SHT31::crc(const RawData_t &data, size_t index) {
  uint8_t crc = 0xFF;

  for (uint32_t j = 0; j < 2; j++) {
    // crc ^= *data++;
    crc ^= data[j + index];

    for (uint32_t i = 8; i; --i) {
      crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
    }
  }

  return (crc == data[index + 2]);
}

bool SHT31::detect() {
  auto rc = false;

  clearPreviousError();

  // read out of status register
  _tx = {0xf3, 0x2d};

  // byte 0: register MSB
  // byte 1: register LSB
  // byte 2: crc
  _rx = {0x00, 0x00, 0x00};

  rc = requestData(_tx, _rx);

  return rc;
}

bool SHT31::read() {
  auto rc = false;

  _tx = {
      0x2c, // single-shot measurement, with clock stretching
      0x0d  // medium-repeatability measurement
  };

  _rx = {
      0x00, 0x00, // tempC high byte, low byte
      0x00,       // crc8 of temp
      0x00, 0x00, // relh high byte, low byte
      0x00        // crc8 of relh
  };

  clearPreviousError();

  if (requestData(_tx, _rx, 4)) {
    justSeen();

    if (crc(_rx) && crc(_rx, 3)) {
      // conversion from SHT31 datasheet
      uint16_t stc = (_rx[0] << 8) | _rx[1];
      uint16_t srh = (_rx[3] << 8) | _rx[4];

      float tc = (float)((stc * 175) / 0xffff) - 45;
      float rh = (float)((srh * 100) / 0xffff);

      Sensor_t *reading = new Sensor(id(), tc, rh);

      setReading(reading);

      rc = true;
    } else { // crc did not match
      crcMismatch();
    }
  } else {
    readFailure();
  }

  return rc;
}

} // namespace ruth
