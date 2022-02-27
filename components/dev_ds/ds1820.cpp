/*
    Ruth
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

#include <esp_attr.h>
#include <esp_log.h>

#include "ArduinoJson.h"
#include "celsius_msg.hpp"
#include "crc.hpp"
#include "dev_ds/ds1820.hpp"
#include "ruth_mqtt/mqtt.hpp"

namespace ds {

DS1820::DS1820(const uint8_t *addr) : Device(addr) { _mutable = false; }

IRAM_ATTR bool DS1820::report() {
  auto rc = false;

  auto start_at = esp_timer_get_time();
  rc = convert();
  const uint32_t convert_us = esp_timer_get_time() - start_at;

  start_at = esp_timer_get_time();

  uint16_t raw = 0;
  if (rc) {

    static uint8_t cmd[10];
    static constexpr size_t cmd_len = sizeof(cmd);
    static uint8_t data[9];
    static constexpr size_t data_len = sizeof(data);

    cmd[9] = 0xbe; // DS1820 read scratchpad

    if (matchRomThenRead(cmd, cmd_len, data, data_len) == false) return false;
    auto crc = crc8(data, data_len);
    if (crc != 0x00) {
      ESP_LOGD(ident(), "crc failure: 0x%02x", crc);

      rc = false; // transmission error
    }

    // convert the data from the scratchpad to a raw temperature
    raw = (data[1] << 8) | data[0];

    // 12 bit res is the configuration default (750ms conversion time)
    // at lower res, the low bits are undefined, so zero them depending on sensor configuration
    switch (data[4] & 0x60) {
    case 0x00:
      raw &= ~7; // 9 bit resolution, 93.75 ms
      break;
    case 0x20:
      raw &= ~3; // 10 bit res, 187.5 ms
      break;
    case 0x40:
      raw &= ~1; // 11 bit res, 375 ms
      break;
    }
  }

  if (rc) {
    updateSeenTimestamp();
    const uint32_t read_us = esp_timer_get_time() - start_at;
    auto status = Celsius({ident(), Celsius::Status::OK, (float)raw / 16.0f, read_us, convert_us, 0});
    ruth::MQTT::send(status);
  } else {
    auto status = Celsius({ident(), Celsius::Status::ERROR, 0, 0, 0, busErrorCode()});
    ruth::MQTT::send(status);
  }

  return rc;
}

} // namespace ds
