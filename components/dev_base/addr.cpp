/*
    Ruth
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

#include <cstring>

#include <esp_log.h>
#include <sys/time.h>
#include <time.h>

#include "dev_base/addr.hpp"

namespace device {

Address::Address(uint8_t addr) {
  _bytes[0] = addr;
  _size = 1;
}

Address::Address(uint8_t *addr, uint32_t len) {
  _size = (len <= _capacity) ? len : _capacity;

  memcpy(_bytes, addr, _size);
}

uint32_t Address::len() const { return size(); }
uint32_t Address::size() const { return _size; }

uint8_t Address::firstByte() const { return _bytes[0]; }
uint8_t Address::lastByte() const { return _bytes[_size - 1]; }
uint8_t Address::singleByte() const { return firstByte(); }

// uint32_t Address::max_len() const { return _max_len; }

// support type casting from Address to a plain ole uint_t array
Address::operator uint8_t *() { return _bytes; }

uint8_t Address::operator[](int i) const { return _bytes[i]; }

// NOTE:
//    1. the == ooperator will compare the actual addr and not the pointers
//    2. the lhs argument decides the length of address to compare
bool Address::operator==(const Address &rhs) const {
  return (memcmp(_bytes, rhs._bytes, _capacity) == 0) ? true : false;
}

bool Address::isValid() const { return (_size && _bytes[0] > 0x00) ? true : false; }

const std::unique_ptr<char[]> Address::debug() const {
  auto const max_len = 63;
  std::unique_ptr<char[]> debug_str(new char[max_len + 1]);
  char *str = debug_str.get();
  str[0] = 0x00; // terminate the char array

  snprintf(str, max_len, "Address(0x");

  // append each of the address bytes
  for (auto i = 0; i < _capacity; i++) {
    snprintf(str + i, (_capacity - i), "%02x", _bytes[i]);
  }

  // append the closing paren ')' for readability
  auto curr_len = strlen(str);
  snprintf(str + curr_len, (max_len - curr_len), ")");

  // move (return) the newly created char array to the caller
  return move(debug_str);
}

} // namespace device
