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

#ifndef ruth_dev_ds_hpp
#define ruth_dev_ds_hpp

#include <memory>

#include "dev_ds/hardware.hpp"

namespace ds {
class Device : public Hardware {

public:
  Device(const uint8_t *addr);

  virtual bool execute() { return false; }
  virtual bool report() = 0;

  bool isMutable() const { return _mutable; }

  uint64_t lastSeen() const { return _timestamp; }
  uint32_t updateSeenTimestamp();

protected:
  bool _mutable = false;

private:
  uint64_t _timestamp = 0; // last seen timestamp (microseconds since epoch)
};
} // namespace ds

#endif
