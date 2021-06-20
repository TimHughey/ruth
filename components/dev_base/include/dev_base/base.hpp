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

#ifndef _ruth_device_hpp
#define _ruth_device_hpp

#include <cstdlib>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "dev_base/addr.hpp"
#include "misc/elapsed.hpp"
#include "misc/textbuffer.hpp"

namespace device {

using std::unique_ptr;

// #define RUTH_DEV_ID_MAX 45
// #define RUTH_DEV_DESC_MAX 15

static constexpr size_t id_max = 32;
static constexpr size_t desc_max = 15;

typedef class TextBuffer<id_max> Id;
typedef class TextBuffer<desc_max> Description;

class Base {
public:
  Base() = default;
  Base(const Address &addr);
  virtual ~Base() = default;

  uint8_t *addrBytes();
  const Address &address() const;
  virtual bool available() const;
  void crcMismatch();
  virtual const unique_ptr<char[]> debug();
  void delay(uint32_t ms) const { vTaskDelay(pdMS_TO_TICKS(ms)); }
  const char *description() const { return _desc.c_str(); };
  uint8_t firstAddressByte() const { return _addr.firstByte(); };
  const char *id() const { return _id.c_str(); };
  bool justSeen(bool rc = true);
  uint8_t lastAddressByte() const { return _addr.lastByte(); };
  virtual void makeID() = 0;
  bool matchID(const char *id) const { return _id == id; }
  static size_t maxIdLen() { return id_max; }
  bool missing() const;
  bool notValid() const;
  int readErrors() const { return _read_errors; }
  void readFailure();
  void readStart();
  uint64_t readStop();
  void setDescription(const char *desc) { _desc = desc; };
  void setID(const char *format, ...);
  void setMissingSeconds(uint32_t secs) { _missing_secs = secs; };
  uint8_t singleByteAddress() const { return _addr.firstByte(); };
  bool valid() const;
  int writeErrors() const { return _write_errors; }
  void writeFailure();
  void writeStart();
  uint64_t writeStop();

private:
  Id _id;        // unique identifier of this device
  Address _addr; // address of this device

protected:
  Description _desc;
  time_t _last_seen = 0; // mtime of last time this device was discovered

  ruth::elapsedMicros _read_us;
  ruth::elapsedMicros _write_us;

  time_t _read_timestamp = 0;

  int _crc_mismatches = 0;
  int _read_errors = 0;
  int _write_errors = 0;
  int _missing_secs = 21;
};
} // namespace device

#endif
