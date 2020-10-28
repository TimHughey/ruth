/*
    id.hpp - Ruth Address of Device
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

#ifndef _ruth_dev_addr_hpp
#define _ruth_dev_addr_hpp

#include <memory>
#include <string>
#include <vector>

using std::unique_ptr;

namespace ruth {

using std::vector;

typedef class DeviceAddress DeviceAddress_t;

class DeviceAddress {
private:
  static const uint32_t _max_len = 10;
  vector<uint8_t> _addr;

public:
  static const int max_addr_len = _max_len;
  DeviceAddress(){};
  // construct a very simple device address of only one byte
  DeviceAddress(uint8_t addr);
  // construct a slightly more complex device of a multi byte address
  DeviceAddress(uint8_t *addr, uint32_t len);

  uint32_t len() const;
  uint8_t firstAddressByte() const;
  // uint8_t addressByteByIndex(uint32_t index);
  uint8_t lastAddressByte() const;
  uint32_t max_len() const;
  bool isValid() const;

  // support type casting from DeviceAddress to a plain ole uint8_t array
  operator uint8_t *();

  uint8_t operator[](int i);

  bool operator==(const DeviceAddress_t &rhs);

  const unique_ptr<char[]> debug();

private:
};
} // namespace ruth

#endif // dev_addr_hpp