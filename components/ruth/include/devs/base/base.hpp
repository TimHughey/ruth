/*
    Device.h - Ruth Common Device for Engines
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
#include <memory>
#include <string>

// #include <freertos/FreeRTOS.h>
#include <sys/time.h>
#include <time.h>

#include "devs/base/addr.hpp"
#include "external/ArduinoJson.h"
#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "readings/readings.hpp"

namespace ruth {

using std::unique_ptr;
using namespace reading;

typedef class Device Device_t;
class Device {
public:
  Device() {} // all values are defaulted in definition of class(es)

  Device(const DeviceAddress_t &addr);
  Device(const string_t &id, const DeviceAddress_t &addr);
  // Device(const Device_t &dev); // copy constructor
  virtual ~Device(); // base class will handle deleting the reading, if needed

  // operators
  bool operator==(Device_t *rhs) const;

  const DeviceAddress_t &address() const;
  uint8_t *addrBytes();

  bool valid() const;
  bool notValid() const;

  // updaters
  void justSeen();

  uint8_t singleByteAddress() const { return _addr.firstByte(); };
  uint8_t firstAddressByte() const { return _addr.firstByte(); };
  uint8_t lastAddressByte() const { return _addr.lastByte(); };

  static size_t maxIdLen() { return 63; }
  virtual void makeID() = 0;
  void setID(const string_t &new_id);
  const string_t &id() const { return _id; };

  // description of device
  void setDescription(const string_t &desc) { _desc = desc; };
  void setDescription(const char *desc) { _desc = desc; };
  const string_t &description() const { return _desc; };

  void setReading(Reading_t *reading);
  void setReadingCmdAck(uint32_t latency_us, const RefID_t &refid);
  void setReadingCmdAck(uint32_t latency_us, const char *refid);

  Reading_t *reading();

  // metrics functions
  void readStart();
  uint64_t readStop();
  void writeStart();
  uint64_t writeStop();

  void setMissingSeconds(uint32_t missing_secs) {
    _missing_secs = missing_secs;
  };
  bool available() const;
  bool missing() const;

  void crcMismatch();
  void readFailure();
  void writeFailure();

  int readErrors() const { return _read_errors; }
  int writeErrors() const { return _write_errors; }

  // delay task for milliseconds
  void delay(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

  virtual const unique_ptr<char[]> debug();

private:
  string_t _id;          // unique identifier of this device
  DeviceAddress_t _addr; // address of this device
  string_t _desc;

protected:
  Reading_t *_reading = nullptr;

  time_t _last_seen = 0; // mtime of last time this device was discovered

  elapsedMicros _read_us;
  elapsedMicros _write_us;

  time_t _read_timestamp = 0;

  int _crc_mismatches = 0;
  int _read_errors = 0;
  int _write_errors = 0;
  int _missing_secs = 45;
};
} // namespace ruth

#endif // Device_h
