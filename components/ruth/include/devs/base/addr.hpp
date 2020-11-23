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

using std::unique_ptr;

namespace ruth {

typedef class DeviceAddress DeviceAddress_t;

class DeviceAddress {

public:
  DeviceAddress(){};
  // construct a very simple device address of only one byte
  DeviceAddress(uint8_t addr);
  // construct a slightly more complex device of a multi byte address
  DeviceAddress(uint8_t *addr, uint32_t len);

  uint32_t len() const;
  uint32_t size() const;

  uint8_t firstByte() const;
  uint8_t lastByte() const;
  uint8_t singleByte() const;

  bool isValid() const;

  // support type casting from DeviceAddress to a plain ole uint8_t array
  operator uint8_t *();

  uint8_t operator[](int i) const;

  bool operator==(const DeviceAddress_t &rhs) const;

  const unique_ptr<char[]> debug() const;

private:
  static const size_t _capacity = 10;
  uint8_t _bytes[_capacity] = {};
  size_t _size = 0;
};
} // namespace ruth

#endif // dev_addr_hpp
