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

#include <freertos/FreeRTOS.h>

#include "message/in.hpp"

namespace ds {
class Device {

public:
  enum Convert : uint32_t { CHECK_TICKS = pdMS_TO_TICKS(30), TIMEOUT = 800000 };
  enum Notifies : uint32_t { BUS_NEEDED = 0xb000, BUS_RELEASED = 0xb001 };

public:
  Device(const uint8_t *rom_code);

  static bool acquireBus(uint32_t timeout_ms = UINT32_MAX);
  inline const uint8_t *addr() const { return _addr; }
  inline size_t addrLen() const { return _addr_max_len; }
  inline uint8_t crc() const { return _addr[AddressIndex::CRC]; }
  virtual bool execute(message::InWrapped msg) { return false; }
  inline uint8_t family() const { return _addr[AddressIndex::FAMILY]; }
  const char *ident() const { return _ident; }
  static size_t identMaxLen() { return _ident_max_len; }
  static bool initBus(uint32_t convert_frequency_ms);
  bool isMutable() const { return _mutable; }

  uint64_t lastSeen() const { return _timestamp; }

  virtual bool report() = 0;
  static bool releaseBus();

  static bool search(uint8_t *rom_code);
  static void setReportFrequency(uint32_t micros);

  uint32_t updateSeenTimestamp();

protected:
  typedef uint8_t *Bytes;
  typedef const size_t Len;

protected:
  static uint8_t busErrorCode();
  static bool convert();
  bool matchRomThenRead(Bytes write, Len write_len, Bytes read, Len read_len);

  static bool resetBus();

protected:
  bool _mutable = false;

  // byte 0: family code, bytes 1-6: serial number, byte & crc
  static constexpr size_t _addr_max_len = 8;
  uint8_t _addr[_addr_max_len];

  static constexpr size_t _ident_max_len = 18;
  char _ident[_ident_max_len];
  bool _needs_convert;

private:
  static bool ensureBus();
  void makeID();

private:
  enum AddressIndex : size_t { FAMILY = 0, SERIAL_START = 1, SERIAL_END = 6, CRC = 7 };

private:
  int64_t _timestamp = 0; // last seen timestamp (microseconds since epoch)
};
} // namespace ds

#endif
