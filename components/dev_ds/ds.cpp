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

#include "dev_ds/ds.hpp"

namespace ds {

Device::Device(const uint8_t *addr) : Hardware(addr), _timestamp(now()) {}

IRAM_ATTR uint32_t Device::updateSeenTimestamp() {
  auto now_ms = now();
  auto diff = now_ms - _timestamp;
  _timestamp = now_ms;

  return diff;
}

} // namespace ds
