/*
    ds_dev.cpp - Ruth Dalla Semiconductor Device
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

#include "devs/ds/dev.hpp"

namespace ruth {

DsDevice::DsDevice(DeviceAddress_t &addr, bool power) : Device(addr) {
  // byte   0: 8-bit family code
  // byte 1-6: 48-bit unique serial number
  // byte   7: crc
  _power = power;

  setDescription(familyDescription());

  makeID();
};

void DsDevice::makeID() {
  //                 00000000001111111
  //       byte num: 01234567890123456
  //     exmaple id: ds/28ffa442711604
  // format of name: ds/famil code + 48-bit serial (without the crc)
  //      total len: 18 bytes (id + string terminator)
  auto addr = address();
  setID("ds/%02x%02x%02x%02x%02x%02x%02x", addr[0], // byte 0: family code
        addr[1], addr[2], addr[3],                  // byte 1-3: serial number
        addr[4], addr[5], addr[6]);                 // byte 4-6: serial number
}

uint8_t DsDevice::family() const { return firstAddressByte(); };
uint8_t DsDevice::crc() { return lastAddressByte(); };
void DsDevice::copyAddrToCmd(uint8_t *cmd) {
  memcpy((cmd + 1), (uint8_t *)addrBytes(), address().size());
}

bool DsDevice::isPowered() { return _power; };
Reading_t *DsDevice::reading() { return _reading; };

bool DsDevice::isDS1820() {
  auto rc = false;

  switch (family()) {
  case 0x10:
  case 0x22:
  case 0x28:
    rc = true;
    break;
  default:
    rc = false;
  }

  return rc;
}
bool DsDevice::isDS2406() {
  return (family() == _family_DS2406) ? true : false;
};
bool DsDevice::isDS2408() {
  return (family() == _family_DS2408) ? true : false;
};
bool DsDevice::isDS2413() {
  return (family() == _family_DS2413) ? true : false;
};
bool DsDevice::isDS2438() {
  return (family() == _family_DS2413) ? true : false;
};

bool DsDevice::hasTemperature() { return isDS1820(); }

const char *DsDevice::familyDescription() const {
  return familyDescription(family());
}

const char *DsDevice::familyDescription(uint8_t family) const {
  switch (family) {
  case 0x10:
  case 0x22:
  case 0x28:
    return "ds1820";

  case 0x29:
    return "ds2408";

  case 0x12:
    return "ds2406";

  case 0x3a:
    return "ds2413";

  case 0x26:
    return "ds2438";

  default:
    return "dsXXXX";
  }
};

void DsDevice::logPresenceFailed() {}

const unique_ptr<char[]> DsDevice::debug() {
  const auto max_len = 63;
  unique_ptr<char[]> debug_str(new char[max_len + 1]);

  snprintf(debug_str.get(), max_len, "DsDevice(family=%s %s)",
           familyDescription(), address().debug().get());

  return move(debug_str);
}
} // namespace ruth
