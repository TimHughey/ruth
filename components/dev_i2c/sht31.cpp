/*
    Ruth
    Copyright (C) 2021  Tim Hughey

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

#include "dev_i2c/sht31.hpp"
#include "dev_i2c/bus.hpp"
#include "dev_i2c/relhum_msg.hpp"
#include "ruth_mqtt/mqtt.hpp"

#include <driver/i2c.h>
#include <esp_attr.h>
#include <esp_log.h>
#include <esp_timer.h>

namespace ruth {
namespace i2c {
static const char *dev_description = "sht31";

SHT31::SHT31(uint8_t addr) : Device(addr, dev_description) {}

IRAM_ATTR bool SHT31::crc(const uint8_t *data, size_t index) {
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

IRAM_ATTR bool SHT31::report() {
  auto rc = false;
  auto start_at = esp_timer_get_time();

  constexpr uint8_t single_shot = 0x2c; // with clock stretching
  constexpr uint8_t medium_repeatability = 0x0d;
  uint8_t tx[] = {
      single_shot,         // single-shot measurement, with clock stretching
      medium_repeatability // medium-repeatability measurement
  };

  uint8_t rx[] = {
      0x00, 0x00, // tempC high byte, low byte
      0x00,       // crc8 of temp
      0x00, 0x00, // relh high byte, low byte
      0x00        // crc8 of relh
  };

  auto cmd = Bus::createCmd();

  // queue the WRITE for device and check for ACK
  i2c_master_write_byte(cmd, writeAddr(), true);

  // queue the device command bytes
  i2c_master_write(cmd, tx, sizeof(tx), I2C_MASTER_ACK);

  // start a new command sequence without sending a stop
  i2c_master_start(cmd);

  // queue the READ for device and check for ACK
  i2c_master_write_byte(cmd, readAddr(), true);

  // queue the READ of number of bytes
  i2c_master_read(cmd, rx, sizeof(rx), I2C_MASTER_LAST_NACK);

  // always queue the stop command
  i2c_master_stop(cmd);

  // clock stretching is leveraged in the event the device requires time
  // to execute the command (e.g. temperature conversion)
  // use timeout scale to adjust time to wait for clock, if needed
  rc = Bus::executeCmd(cmd, 5.0);

  if (rc) {
    if (crc(rx) && crc(rx, 3)) {
      // conversion from SHT31 datasheet
      uint16_t stc = (rx[0] << 8) | rx[1];
      uint16_t srh = (rx[3] << 8) | rx[4];

      float tc = (float)((stc * 175) / 0xffff) - 45;
      float rh = (float)((srh * 100) / 0xffff);

      const auto read_us = esp_timer_get_time() - start_at;

      auto status = RelHum({_ident, RelHum::Status::OK, tc, rh, read_us, 0});
      ruth::MQTT::send(status);
    } else { // crc did not match
      auto status = RelHum({_ident, RelHum::Status::CRC_MISMATCH, 0, 0, 0, 0});
      ruth::MQTT::send(status);
    }
  }

  return rc;
}

} // namespace i2c
} // namespace ruth
