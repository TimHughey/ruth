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

#include <driver/i2c.h>
#include <esp_log.h>

#include "bus.hpp"
#include "dev_i2c/sht31.hpp"
#include "relhum_msg.hpp"
#include "ruth_mqtt/mqtt.hpp"

namespace i2c {
static const char *dev_description = "sht31";

SHT31::SHT31(uint8_t addr) : Device(addr, dev_description) {}

bool SHT31::crc(const uint8_t *data, size_t index) {
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

bool SHT31::detect() { return report(false); }

bool SHT31::report(const bool send) {
  auto rc = false;
  auto start_at = now();

  uint8_t tx[] = {
      0x2c, // single-shot measurement, with clock stretching
      0x0d  // medium-repeatability measurement
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

  // clock stretching is leveraged in the event the device requires time
  // to execute the command (e.g. temperature conversion)
  // use timeout scale to adjust time to wait for clock, if needed

  // start a new command sequence without sending a stop
  i2c_master_start(cmd);

  // queue the READ for device and check for ACK
  i2c_master_write_byte(cmd, readAddr(), true);

  // queue the READ of number of bytes
  i2c_master_read(cmd, rx, sizeof(rx), I2C_MASTER_LAST_NACK);

  // always queue the stop command
  i2c_master_stop(cmd);

  rc = Bus::executeCmd(cmd, 4.0);

  if (send == false) return rc;

  if (rc) {
    if (crc(rx) && crc(rx, 3)) {
      // conversion from SHT31 datasheet
      uint16_t stc = (rx[0] << 8) | rx[1];
      uint16_t srh = (rx[3] << 8) | rx[4];

      float tc = (float)((stc * 175) / 0xffff) - 45;
      float rh = (float)((srh * 100) / 0xffff);

      const auto read_us = now() - start_at;

      updateSeenTimestamp();
      auto status = RelHum({_ident, RelHum::Status::OK, tc, rh, read_us, 0});
      ruth::MQTT::send(status);
    } else { // crc did not match
      auto status = RelHum({_ident, RelHum::Status::CRC_MISMATCH, 0, 0, 0, 0});
      ruth::MQTT::send(status);
    }
  } else {
    auto status = RelHum({_ident, RelHum::Status::ERROR, 0, 0, 0, Bus::errorCode()});
    ruth::MQTT::send(status);
  }

  return rc;
}

} // namespace i2c
