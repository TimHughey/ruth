/*
    devs/base.hpp
    Ruth Common Device for Engines
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

#include "devs/base/addr.hpp"
#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "readings/readings.hpp"

namespace ruth {

using std::unique_ptr;
using namespace reading;

#define RUTH_DEV_ID_MAX 45
#define RUTH_DEV_DESC_MAX 15

typedef class TextBuffer<RUTH_DEV_ID_MAX> DeviceId_t;
typedef class TextBuffer<RUTH_DEV_DESC_MAX> DeviceDescription_t;
typedef class Device Device_t;

class Device {
public:
  Device() {} // all values are defaulted in definition of class(es)

  Device(const DeviceAddress_t &addr);
  virtual ~Device(); // base class will handle deleting the reading, if needed

  const DeviceAddress_t &address() const;
  uint8_t *addrBytes();

  bool valid() const;
  bool notValid() const;

  // updaters
  bool justSeen(bool rc = true);

  uint8_t singleByteAddress() const { return _addr.firstByte(); };
  uint8_t firstAddressByte() const { return _addr.firstByte(); };
  uint8_t lastAddressByte() const { return _addr.lastByte(); };

  static size_t maxIdLen() { return RUTH_DEV_ID_MAX; }
  virtual void makeID() = 0;
  bool matchID(const char *id) const { return _id == id; }
  void setID(const char *format, ...);
  const char *id() const { return _id.c_str(); };

  // description of device
  void setDescription(const char *desc) { _desc = desc; };
  const char *description() const { return _desc.c_str(); };

  void setReading(Reading_t *reading);
  void setReadingCmdAck(uint32_t latency_us, const RefID_t &refid);
  void setReadingCmdAck(uint32_t latency_us, const char *refid);

  Reading_t *reading();

  // metrics functions
  void readStart();
  uint64_t readStop();
  void writeStart();
  uint64_t writeStop();

  void setMissingSeconds(uint32_t secs) { _missing_secs = secs; };
  virtual bool available() const;
  bool missing() const;

  void crcMismatch();
  void readFailure();
  void writeFailure();

  int readErrors() const { return _read_errors; }
  int writeErrors() const { return _write_errors; }

  // delay task for milliseconds
  void delay(uint32_t ms) const { vTaskDelay(pdMS_TO_TICKS(ms)); }

  virtual const unique_ptr<char[]> debug();

private:
  DeviceId_t _id;        // unique identifier of this device
  DeviceAddress_t _addr; // address of this device
  DeviceDescription_t _desc;

protected:
  Reading_t *_reading = nullptr;

  time_t _last_seen = 0; // mtime of last time this device was discovered

  elapsedMicros _read_us;
  elapsedMicros _write_us;

  time_t _read_timestamp = 0;

  int _crc_mismatches = 0;
  int _read_errors = 0;
  int _write_errors = 0;
  int _missing_secs = 21;
};
} // namespace ruth

#endif // Device_h
