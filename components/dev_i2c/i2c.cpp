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

#include <esp_attr.h>
#include <esp_log.h>

#include "bus.hpp"
#include "dev_i2c/i2c.hpp"

namespace i2c {

static const char *unique_id = nullptr;

IRAM_ATTR Device::Device(const uint8_t addr, const char *description, const bool is_mutable)
    : _addr(addr), _mutable(is_mutable), _description(description) {
  _seen_at = esp_timer_get_time();
  makeID();
}

IRAM_ATTR void Device::delay(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

bool Device::initHardware() { return Bus::init(); }

IRAM_ATTR void Device::makeID() {
  auto *p = _ident;
  *p++ = 'i';
  *p++ = '2';
  *p++ = 'c';
  *p++ = '.';

  // ensure there is remaining space for the dot and addr of device
  p = (char *)memccpy(p, unique_id, 0x00, _ident_max_len - 8);

  p--; // memccpy returns a pointer to the copied null, back up one
  *p++ = '.';

  const size_t remaining = _ident_max_len - (p - _ident);
  p = (char *)memccpy(p, description(), 0x00, remaining);

  p--;
  *p++ = '.';

  if (_addr < 0x10) *p++ = '0';       // zero pad values less than 0x10
  itoa(_addr, p, 16);                 // convert to hex
  p = (_addr < 0x10) ? p + 1 : p + 2; // move pointer forward based on zero padding

  *p = 0x00; // null terminate the ident
}

IRAM_ATTR int Device::readAddr() const { return (_addr << 1) | I2C_MASTER_READ; }

void Device::setUniqueId(const char *id) { unique_id = id; }

IRAM_ATTR int Device::writeAddr() const { return (_addr << 1) | I2C_MASTER_WRITE; }

} // namespace i2c
