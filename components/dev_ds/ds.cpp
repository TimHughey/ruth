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

#include "dev_ds/ds.hpp"
#include "dev_ds/bus.hpp"

#include <esp_attr.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace ruth {
namespace ds {
static const char *TAG = "ds::device";

inline auto now() { return esp_timer_get_time(); }

Device::Device(const uint8_t *rom_code) : _timestamp(now()) {
  // copy the rom code to the address
  memcpy(_addr, rom_code, sizeof(_addr));

  makeID();

  // determine if this device requires a bus convert
  switch (family()) {
  case 0x28:
    _needs_convert = true;
    break;

  default:
    _needs_convert = false;
  }
}

IRAM_ATTR bool Device::acquireBus(uint32_t timeout_ms) { return Bus::acquire(timeout_ms); }

uint8_t Device::busErrorCode() { return Bus::lastStatus(); }

static uint32_t convert_micros = 0;
static int64_t convert_last = 0;

IRAM_ATTR bool Device::convert() {
  bool complete = false;
  const auto start_at = now();

  // each device calls convert as part of it's status report.
  // but converts are a global operation on all temperature devices on the bus.
  // prevent repeated converts with a minimum time of 50% the reporting cycle.
  const auto min_interval = convert_micros >> 1;
  const auto since_last_convert = start_at - convert_last;
  if (since_last_convert < min_interval) {

    return true;
  }

  while (Bus::convert(complete)) {
    if (complete)
      break; // convert is complete

    vTaskDelay(Convert::CHECK_TICKS); // delay between checks

    if ((now() - start_at) > Convert::TIMEOUT) {
      ESP_LOGW(TAG, "convert timeout");
      Bus::convert(complete, true);
      break;
    }
  }

  // once a convert (success or failure) is complete note the time to prevent subsequent converts
  // before the next reporting cycle
  convert_last = now();

  return complete;
}

bool Device::initBus(uint32_t convert_frequency_ms) {
  // the value passed is in ms and we need micros
  convert_micros = 1000 * convert_frequency_ms;
  // ensure the first call to convert performs a convert
  convert_last = now() - convert_micros;

  return Bus::ensure();
}

void Device::makeID() {
  auto *p = _ident;
  *p++ = 'd';
  *p++ = 's';
  *p++ = '.';

  constexpr size_t addr_bytes_for_ident = sizeof(_addr) - 1; // crc is not part of the ident
  for (size_t i = 0; i < addr_bytes_for_ident; i++) {
    const uint8_t byte = _addr[i];

    // this is knownly duplicated code to avoid creating dependencies
    if (byte < 0x10)
      *p++ = '0';                      // zero pad values less than 0x10
    itoa(byte, p, 16);                 // convert to hex
    p = (byte < 0x10) ? p + 1 : p + 2; // move pointer forward based on zero padding
  }

  *p = 0x00; // null terminate the ident
}

IRAM_ATTR bool Device::matchRomThenRead(Bytes write, Len wlen, Bytes read, Len rlen) {
  // setup the match rom command but don't touch byte 9 which is the device specific command
  auto *p = write;
  *p++ = 0x55;                  // byte 0: match rom
  memcpy(p, addr(), addrLen()); // bytes 1-8: rom to match

  return Bus::writeThenRead(write, wlen, read, rlen);
}

IRAM_ATTR bool Device::releaseBus() { return Bus::release(); }

IRAM_ATTR bool Device::resetBus() { return Bus::reset(); }

IRAM_ATTR bool Device::search(uint8_t *rom_code) { return Bus::search(rom_code); }

IRAM_ATTR uint32_t Device::updateSeenTimestamp() {
  const auto now_us = now();
  const auto diff = now_us - _timestamp;

  _timestamp = now_us;

  return diff;
}

} // namespace ds
} // namespace ruth
