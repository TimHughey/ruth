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

#include <cstring>
#include <ctime>
#include <sys/time.h>

#include <esp_attr.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "bus.hpp"
#include "dev_ds/hardware.hpp"

namespace ds {

static const char *TAG = "ds:hardware";

Hardware::Hardware(const uint8_t *rom_code) {
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

static uint64_t convert_micros = 0;
static uint64_t convert_last = 0;

IRAM_ATTR bool Hardware::convert() {
  bool complete = false;
  const uint64_t start_at = now();

  // each device calls convert as part of it's status report.
  // but converts are a global operation on all temperature devices on the bus.
  // prevent repeated converts with a minimum time of 50% the reporting cycle.
  const uint64_t min_interval = convert_micros >> 1;
  const uint64_t since_last_convert = start_at - convert_last;
  if (since_last_convert < min_interval) {

    return true;
  }

  while (Bus::convert(complete)) {
    if (complete) break; // convert is complete

    vTaskDelay(Convert::CHECK_TICKS); // delay between checks

    if ((now() - start_at) > Convert::TIMEOUT) {
      ESP_LOGW(TAG, "convert timeout");
      Bus::convert(complete, true);
    }
  }

  // once a convert (success or failure) is complete note the time to prevent subsequent converts
  // before the next reporting cycle
  convert_last = now();

  return complete;
}

bool Hardware::initHardware(uint32_t convert_frequency_ms) {
  convert_micros = 1000 * convert_frequency_ms; // the value passed is in ms and we need micros
  convert_last = now() - convert_micros;        // ensure the first call to convert performs a convert

  return Bus::ensure();
}

void Hardware::makeID() {
  auto *p = _ident;
  *p++ = 'd';
  *p++ = 's';

  constexpr size_t addr_bytes_for_ident = sizeof(_addr) - 1; // crc is not part of the ident
  for (size_t i = 0; i < addr_bytes_for_ident; i++) {
    const uint8_t byte = _addr[i];

    // this is knownly duplicated code to avoid creating dependencies
    if (byte < 0x10) *p++ = '0';       // zero pad when less than 0x10
    itoa(byte, p, 16);                 // convert to hex
    p = (byte < 0x10) ? p + 1 : p + 2; // move pointer forward based on zero padding
  }

  *p = 0x00; // zero terminate
}

IRAM_ATTR bool Hardware::matchRomThenRead(Bytes write, Len wlen, Bytes read, Len rlen) {

  // setup the match rom command but don't touch byte 9 which is the device specific command
  auto *p = write;
  *p++ = 0x55;                  // byte 0: match rom
  memcpy(p, addr(), addrLen()); // bytes 1-8: rom to match

  return Bus::writeThenRead(write, wlen, read, rlen);
}

static struct timeval time_now;
IRAM_ATTR uint64_t Hardware::now() {

  uint64_t us_since_epoch;

  gettimeofday(&time_now, nullptr);

  us_since_epoch = 0;
  us_since_epoch += time_now.tv_sec * 1000000L; // convert seconds to microseconds
  us_since_epoch += time_now.tv_usec;           // add microseconds since last second

  return us_since_epoch;
}

IRAM_ATTR bool Hardware::search(uint8_t *rom_code) { return Bus::search(rom_code); }
} // namespace ds
