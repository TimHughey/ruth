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

#include <memory>
#include <string>

#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <sys/time.h>
#include <time.h>

#include "devs/base/base.hpp"
#include "devs/i2c/sht31.hpp"
#include "local/types.hpp"
#include "net/network.hpp"

namespace ruth {

SHT31::SHT31() : I2cDevice(0x44) {}

bool SHT31::crc(const uint8_t *data) {
  uint8_t crc = 0xFF;

  for (uint32_t j = 2; j; --j) {
    crc ^= *data++;

    for (uint32_t i = 8; i; --i) {
      crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
    }
  }

  // data was ++ in the above loop so it is already pointing at the crc
  return (crc == *data);
}

bool SHT31::detectOnBus() {

  _esp_rc_prev = ESP_OK;

  _tx = {
      0x2c, // single-shot measurement, with clock stretching
      0x06  // high-repeatability measurement (max duration 15ms)
  };

  _rx = {
      0x00, 0x00, // tempC high byte, low byte
      0x00,       // crc8 of temp
      0x00, 0x00, // relh high byte, low byte
      0x00        // crc8 of relh
  };

  if (requestData(1500) == ESP_OK) {
    return true;
  }

  return false;
}

bool SHT31::read() {
  auto rc = false;

  _tx = {
      0x2c, // single-shot measurement, with clock stretching
      0x06  // high-repeatability measurement (max duration 15ms)
  };

  _rx = {
      0x00, 0x00, // tempC high byte, low byte
      0x00,       // crc8 of temp
      0x00, 0x00, // relh high byte, low byte
      0x00        // crc8 of relh
  };

  _esp_rc = requestData();

  if (_esp_rc == ESP_OK) {
    justSeen();

    if (crc(_rx.data()) && crc(&(_rx[3]))) {
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
  } else { // esp_rc != ESP_OK
    readFailure();
  }

  return rc;
}

} // namespace ruth
