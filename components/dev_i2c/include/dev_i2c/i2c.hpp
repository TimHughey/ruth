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

#pragma once

#include <memory>

#include "message/in.hpp"

namespace ruth {
namespace i2c {
class Device {
public:
  static constexpr bool MUTABLE = true;
  static constexpr bool IMMUTABLE = false;

public:
  Device(const uint8_t addr, const char *description, const bool is_mutable = IMMUTABLE);
  virtual ~Device() = default;

  uint8_t addr() const { return _addr; }
  static void delay(uint32_t ms);
  const char *description() const { return _description; };
  virtual bool execute(message::InWrapped msg) { return false; }

  const char *ident() const { return _ident; }
  static constexpr size_t identMaxLen() { return _ident_max_len; }
  static bool initHardware();
  bool isMutable() const { return _mutable; }

  void makeID();
  virtual bool report() = 0;
  static void setUniqueId(const char *);
  uint32_t seen();
  int64_t seenLast() const { return _seen_at; }

protected:
  int readAddr() const;
  int writeAddr() const;

protected:
  static constexpr size_t _ident_max_len = 45;

  const uint8_t _addr;
  const bool _mutable;
  char _ident[_ident_max_len];

private:
  int64_t _seen_at; // µs since boot
  const char *_description = nullptr;
};

} // namespace i2c
} // namespace ruth
