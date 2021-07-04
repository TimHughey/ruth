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

#ifndef ruth_ds_hardware_hpp
#define ruth_ds_hardware_hpp

#include <memory>

#include <freertos/FreeRTOS.h>

namespace ds {
class Hardware {
public:
  enum AddressIndex : size_t { FAMILY = 0, SERIAL_START = 1, SERIAL_END = 6, CRC = 7 };
  enum Convert : uint32_t { CHECK_TICKS = pdMS_TO_TICKS(30), TIMEOUT = 800000 };
  enum Notifies : uint32_t { BUS_NEEDED = 0xb000, BUS_RELEASED = 0xb001 };

public:
  Hardware(const uint8_t *rom_code);

  static bool acquireBus(uint32_t timeout_ms = UINT32_MAX);
  const uint8_t *addr() const { return _addr; }
  size_t addrLen() const { return sizeof(_addr); }

  inline uint8_t crc() const { return _addr[AddressIndex::CRC]; }
  inline uint8_t family() const { return _addr[AddressIndex::FAMILY]; }

  const char *ident() const { return _ident; }
  static size_t identMaxLen() { return _ident_max_len; }
  static bool initHardware(uint32_t convert_frequency_ms);
  static uint64_t now();

  static bool releaseBus();

  static bool search(uint8_t *rom_code);
  static void setReportFrequency(uint32_t micros);

  // static bool wantBusNow();
  // static bool willGiveBusDelay(const uint32_t delay_ms, bool &busTaken);

protected:
  typedef uint8_t *Bytes;
  typedef const size_t Len;

protected:
  static uint8_t busErrorCode();
  static bool convert();
  bool matchRomThenRead(Bytes write, Len write_len, Bytes read, Len read_len);

  static inline bool resetBus();

private:
  static bool ensureBus();
  void makeID();

private:
  static constexpr size_t _ident_max_len = 18;
  // byte 0: family code, bytes 1-6: serial number, byte & crc
  uint8_t _addr[8];
  char _ident[_ident_max_len];
  bool _needs_convert;

  // char _status[32];
};
} // namespace ds

#endif // pwm_dev_hpp
