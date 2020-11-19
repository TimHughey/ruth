/*
    ds_dev.h - Ruth Dalla Semiconductor Device
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

#ifndef _ruth_ds_dev_hpp
#define _ruth_ds_dev_hpp

#include <memory>
#include <string>

#include "devs/base/base.hpp"

namespace ruth {

typedef class DsDevice DsDevice_t;

class DsDevice : public Device {
private:
  static const uint8_t _family_byte = 0;
  static const uint8_t _crc_byte = 7;

  static const uint8_t _family_DS2408 = 0x29;
  static const uint8_t _family_DS2406 = 0x12;
  static const uint8_t _family_DS2413 = 0x3a;
  static const uint8_t _family_DS2438 = 0x26;

  bool _power = false; // is the device powered?

  const char *familyDescription(uint8_t family) const;
  const char *familyDescription() const;

public:
  DsDevice();
  DsDevice(DeviceAddress_t &addr, bool power = false);

  void makeID();

  uint8_t family() const;
  uint8_t crc();
  void copyAddrToCmd(uint8_t *cmd);
  bool isPowered();
  Reading_t *reading();

  bool hasTemperature();
  bool isDS1820();
  bool isDS2406();
  bool isDS2408();
  bool isDS2413();
  bool isDS2438();

  // info / debug functions
  void logPresenceFailed();

  const unique_ptr<char[]> debug();
};

typedef class DsDevice DsDevice_t;

} // namespace ruth

#endif // ds_dev_h
