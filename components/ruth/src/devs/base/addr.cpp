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

#include <esp_log.h>
#include <sys/time.h>
#include <time.h>

#include "devs/base/addr.hpp"
#include "local/types.hpp"

namespace ruth {

// copy constructor
DeviceAddress::DeviceAddress(const DeviceAddress &rhs) : _bytes(rhs._bytes) {
  _bytes.shrink_to_fit();
}

DeviceAddress::DeviceAddress(uint8_t addr) {
  _bytes.push_back(addr);
  _bytes.shrink_to_fit();
}

DeviceAddress::DeviceAddress(uint8_t *addr, uint32_t len) {
  _bytes.assign(addr, addr + len);
  _bytes.shrink_to_fit();
}

uint32_t DeviceAddress::len() const { return _bytes.size(); }
uint32_t DeviceAddress::size() const { return _bytes.size(); }

uint8_t DeviceAddress::firstByte() const { return _bytes.front(); }
uint8_t DeviceAddress::lastByte() const { return _bytes.back(); }
uint8_t DeviceAddress::singleByte() const { return _bytes.front(); }

// uint32_t DeviceAddress::max_len() const { return _max_len; }

// support type casting from DeviceAddress to a plain ole uint_t array
DeviceAddress::operator uint8_t *() { return _bytes.data(); }

uint8_t DeviceAddress::operator[](int i) const { return _bytes.at(i); }

// NOTE:
//    1. the == ooperator will compare the actual addr and not the pointers
//    2. the lhs argument decides the length of address to compare
bool DeviceAddress::operator==(const DeviceAddress_t &rhs) const {
  return (_bytes == rhs._bytes);
}

bool DeviceAddress::isValid() const {
  if (_bytes.empty() || (_bytes.front() == 0x00))
    return false;

  return true;
}

const std::unique_ptr<char[]> DeviceAddress::debug() const {
  auto const max_len = 63;
  unique_ptr<char[]> debug_str(new char[max_len + 1]);
  char *str = debug_str.get();
  str[0] = 0x00; // terminate the char array
  auto curr_len = strlen(str);

  snprintf(str, max_len, "DeviceAddress(0x");

  // append each of the address bytes
  for_each(_bytes.begin(), _bytes.end(), [this, str](uint8_t byte) {
    auto curr_len = strlen(str);
    snprintf(str + curr_len, (max_len - curr_len), "%02x", byte);
  });

  // append the closing paren ')' for readability
  curr_len = strlen(str);
  snprintf(str + curr_len, (max_len - curr_len), ")");

  // move (return) the newly created char array to the caller
  return move(debug_str);
}

} // namespace ruth
