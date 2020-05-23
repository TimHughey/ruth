/*
    dev_id.cpp - Ruth Address of Device
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
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <esp_log.h>
#include <sys/time.h>
#include <time.h>

#include "devs/addr.hpp"
#include "local/types.hpp"

namespace ruth {

DeviceAddress::DeviceAddress(uint8_t addr) {
  _addr.resize(1);
  _addr.assign(&addr, &addr + 1);
}

DeviceAddress::DeviceAddress(uint8_t *addr, uint32_t len) {
  _addr.reserve(len);
  _addr.assign(addr, addr + len);
}

uint32_t DeviceAddress::len() const { return _addr.size(); }
uint8_t DeviceAddress::firstAddressByte() const { return _addr.front(); }
// uint8_t DeviceAddress::addressByteByIndex(uint32_t index) { return _addr[0]; }
uint8_t DeviceAddress::lastAddressByte() const { return _addr.back(); }
uint32_t DeviceAddress::max_len() const { return _max_len; }

// support type casting from DeviceAddress to a plain ole char array
DeviceAddress::operator uint8_t *() { return _addr.data(); }

uint8_t DeviceAddress::operator[](int i) { return _addr.at(i); }

// NOTE:
//    1. the == ooperator will compare the actual addr and not the pointers
//    2. the lhs argument decides the length of address to compare
bool DeviceAddress::operator==(const DeviceAddress_t &rhs) {
  return (_addr == rhs._addr);
}

bool DeviceAddress::isValid() const {
  if (_addr.empty() || _addr.front() == 0x00)
    return false;

  return true;
}

const std::unique_ptr<char[]> DeviceAddress::debug() {
  auto const max_len = 63;
  unique_ptr<char[]> debug_str(new char[max_len + 1]);
  char *str = debug_str.get();
  str[0] = 0x00; // terminate the char array for string use
  auto curr_len = strlen(str);

  snprintf(str, max_len, "DeviceAddress(0x");

  // append each of the address bytes
  for_each(_addr.begin(), _addr.end(), [this, str](uint8_t byte) {
    auto curr_len = strlen(str);
    snprintf(str + curr_len, (max_len - curr_len), "%02x", byte);
  });

  // append the closing paren ')' for readability
  curr_len = strlen(str);
  snprintf(str + curr_len, (max_len - curr_len), ")");

  // move (return) the newly created string to the caller
  return move(debug_str);
}

} // namespace ruth
