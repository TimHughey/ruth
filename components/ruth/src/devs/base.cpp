/*
     Device.cpp - Master Control Common Device for Engines
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

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ios>
#include <memory>
#include <string>
#include <tuple>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <sys/time.h>
#include <time.h>

#include "devs/addr.hpp"
#include "devs/base.hpp"
#include "misc/local_types.hpp"
#include "readings/readings.hpp"

using std::move;
using std::unique_ptr;

namespace ruth {

// construct a new Device with only an address
Device::Device(DeviceAddress_t &addr) { _addr = addr; }

Device::Device(const std::string &id, DeviceAddress_t &addr) {
  _id = id; // copy id and addr objects
  _addr = addr;
}

// base class will handle deleteing the reading, if needed
Device::~Device() {
  if (_reading)
    delete _reading;
}

bool Device::operator==(Device_t *rhs) const { return (_id == rhs->_id); }

void Device::justSeen() { _last_seen = time(nullptr); }
void Device::setID(const std::string &new_id) { _id = new_id; }
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
    reading->setReadUS(_read_us);
    reading->setWriteErrors(_write_errors);
    reading->setWriteUS(_write_us);
  }

  _reading = reading;
};

void Device::setReadingCmdAck(uint32_t latency_us, RefID_t &refid) {
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

  return _read_us;
}
uint64_t Device::readUS() { return _read_us; }
time_t Device::readTimestamp() { return _read_timestamp; }
time_t Device::timeCreated() { return _created_mtime; }
time_t Device::secondsSinceLastSeen() { return (time(nullptr) - _last_seen); }

bool Device::available() { return (secondsSinceLastSeen() <= _missing_secs); }
bool Device::missing() { return (!available()); }

void Device::writeStart() { _write_us.reset(); }
uint64_t Device::writeStop() {
  _write_us.freeze();

  return _write_us;
}
uint64_t Device::writeUS() { return _write_us; }

void Device::crcMismatch() { _crc_mismatches++; }
void Device::readFailure() { _read_errors++; }
void Device::writeFailure() { _write_errors++; }

// this is a fairly expensive method, avoid production use
const unique_ptr<char[]> Device::debug() {
  auto const max_len = 319;

  // allocate from the heap to minimize task stack impact
  unique_ptr<char[]> debug_str(new char[max_len + 1]);
  unique_ptr<statsMap_t> map_ptr(new statsMap_t);

  // get pointers to increase code readability
  char *str = debug_str.get();
  statsMap_t *map = map_ptr.get();

  // null terminate the char array for use as string buffer
  str[0] = 0x00;
  auto curr_len = strlen(str);

  snprintf(str, max_len, "Device(%s id(%s) desc(%s)", _addr.debug().get(),
           id().c_str(), description().c_str());

  map->insert({"crc_fail", _crc_mismatches});
  map->insert({"err_read", _read_errors});
  map->insert({"err_write", _write_errors});
  map->insert({"us_read", readUS()});
  map->insert({"us_write", writeUS()});

  // append stats that are non-zero
  for_each(map->begin(), map->end(), [this, str](statEntry_t item) {
    std::string &metric = item.first;
    uint32_t val = item.second;

    if (val > 0) {
      auto curr_len = strlen(str);
      char *s = str + curr_len;
      auto max = max_len - curr_len;

      snprintf(s, max, " %s(%u)", metric.c_str(), val);
    }
  });

  // append the closing parenenthesis ')' for readability
  curr_len = strlen(str);
  snprintf(str + curr_len, (max_len - curr_len), ")");

  return move(debug_str);
}
} // namespace ruth
