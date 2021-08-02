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

#ifndef ruth_dev_i2c_hpp
#define ruth_dev_i2c_hpp

#include <memory>

#include "message/in.hpp"

namespace i2c {
class Device {
public:
  static constexpr bool MUTABLE = true;
  static constexpr bool IMMUTABLE = false;

public:
  Device(const uint8_t addr, const char *description, const bool is_mutable = IMMUTABLE);
  virtual ~Device() = default;

  uint8_t addr() const { return _addr; }
  const char *description() const { return _description; };
  static void delay(uint32_t ms);
  virtual bool detect() = 0;
  virtual bool execute(message::InWrapped msg) { return false; }

  const char *ident() const { return _ident; }
  static size_t identMaxLen() { return _ident_max_len; }
  static bool initHardware();
  bool isMutable() const { return _mutable; }
  static void holdBus();
  uint64_t lastSeen() const { return _timestamp; }
  void makeID();
  virtual bool report(const bool send = true) = 0;
  static void setUniqueId(const char *);
  uint32_t updateSeenTimestamp();

protected:
  // static uint64_t now();
  int readAddr() const;
  int writeAddr() const;

protected:
  static constexpr size_t _ident_max_len = 45;

  const uint8_t _addr;
  const bool _mutable;
  char _ident[_ident_max_len];

private:
  int64_t _timestamp; // last seen timestamp (microseconds since boot)
  const char *_description = nullptr;
};
} // namespace i2c

#endif
