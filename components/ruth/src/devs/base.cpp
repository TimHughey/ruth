/*
     base.cpp - Ruth Base Device for Engines
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

#include <cstdlib>
#include <memory>
#include <string>

#include <sys/time.h>
#include <time.h>

#include "devs/addr.hpp"
#include "devs/base.hpp"
#include "local/types.hpp"
#include "readings/readings.hpp"

namespace ruth {

using std::move;
using std::unique_ptr;

// construct a new Device with only an address
Device::Device(DeviceAddress_t &addr) { _addr = addr; }

Device::Device(const string_t &id, DeviceAddress_t &addr) {
  _id = id; // copy id and addr objects
  _addr = addr;
}

// base class will handle deleteing the reading, if needed
Device::~Device() {
  if (_reading)
    delete _reading;
}

bool Device::operator==(Device_t *rhs) const { return (_id == rhs->_id); }

void Device::justSeen() {
  auto was_missing = missing();

  _last_seen = time(nullptr);

  if (was_missing) {
    textReading::rlog("missing device \"%s\" has returned", _id.c_str());
  }
}
void Device::setID(const string_t &new_id) { _id = new_id; }
void Device::setID(char *new_id) { _id = new_id; }

// updaters
void Device::setReading(Reading_t *reading) {
  if (_reading != nullptr) {
    delete _reading;
    _reading = nullptr;
  }

  if (reading) {
    reading->setCRCMismatches(_crc_mismatches);
    reading->setReadErrors(_read_errors);
    reading->setReadUS((uint64_t)_read_us);
    reading->setWriteErrors(_write_errors);
    reading->setWriteUS((uint64_t)_write_us);

    // when there is an actual reading mark the device as just seen
    justSeen();
  }

  _reading = reading;
};

void Device::setReadingCmdAck(uint32_t latency_us, const RefID_t &refid) {
  if (_reading != nullptr) {
    _reading->setCmdAck(latency_us, refid);
  }
}

void Device::setReadingCmdAck(uint32_t latency_us, const char *refid) {
  if (_reading != nullptr) {
    _reading->setCmdAck(latency_us, refid);
  }
}

uint8_t Device::firstAddressByte() { return _addr.firstAddressByte(); };
uint8_t Device::lastAddressByte() { return _addr.lastAddressByte(); };
DeviceAddress_t &Device::addr() { return _addr; }
uint8_t *Device::addrBytes() { return (uint8_t *)_addr; }
Reading_t *Device::reading() { return _reading; }
uint32_t Device::idMaxLen() { return _id_len; };
bool Device::isValid() { return firstAddressByte() != 0x00 ? true : false; };
bool Device::isNotValid() { return !isValid(); }

// metrics functions
void Device::readStart() { _read_us.reset(); }
uint64_t Device::readStop() {
  _read_us.freeze();
  _read_timestamp = time(nullptr);

  return (uint64_t)_read_us;
}
uint64_t Device::readUS() { return (uint64_t)_read_us; }
time_t Device::readTimestamp() { return _read_timestamp; }
time_t Device::timeCreated() { return _created_mtime; }

bool Device::available() const {
  // last_seen defaults to 0 when constructed (e.g. discovered) so avoid
  // indicating it was missing
  if (_last_seen > 0) {
    auto seen_seconds = time(nullptr) - _last_seen;

    return (seen_seconds <= _missing_secs);
  }

  return true;
}

bool Device::missing() const { return (!available()); }

void Device::writeStart() { _write_us.reset(); }
uint64_t Device::writeStop() {
  _write_us.freeze();

  return (uint64_t)_write_us;
}
uint64_t Device::writeUS() { return (uint64_t)_write_us; }

void Device::crcMismatch() { _crc_mismatches++; }
void Device::readFailure() { _read_errors++; }
void Device::writeFailure() { _write_errors++; }

// this is a fairly expensive method, avoid production use
const unique_ptr<char[]> Device::debug() {
  auto const max_len = 256;
  // allocate from the heap to minimize task stack impact
  unique_ptr<char[]> debug_str(new char[max_len + 1]);

  // get pointers to increase code readability
  char *str = debug_str.get();

  snprintf(str, max_len, "Device \"%s\" id=\"%s\" desc=\"%s\"",
           _addr.debug().get(), id().c_str(), description().c_str());

  return move(debug_str);
}
} // namespace ruth
