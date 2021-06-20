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

#include "dev_base/base.hpp"

namespace device {

using namespace std;

// construct a new Device with only an address
Base::Base(const Address &addr) : _addr(addr) { _last_seen = time(nullptr); }

uint8_t *Base::addrBytes() { return (uint8_t *)_addr; }

const Address &Base::address() const { return _addr; }

bool Base::available() const {
  auto seen_seconds = time(nullptr) - _last_seen;

  return (seen_seconds <= _missing_secs);
}

void Base::crcMismatch() { _crc_mismatches++; }

// this is an expensive method, avoid production use
const unique_ptr<char[]> Base::debug() {
  constexpr size_t max_len = 256;
  // allocate from the heap to minimize task stack impact
  auto debug_str = make_unique<char[]>(max_len);

  snprintf(debug_str.get(), max_len, "Base \"%s\" id=\"%s\" desc=\"%s\"", _addr.debug().get(), id(),
           description());

  return move(debug_str);
}

bool Base::justSeen(bool rc) {
  if (rc == true) {
    if (missing()) {
      // Text::rlog("device \"%s\" was missing %d seconds", _id.c_str(), (time(nullptr) - _last_seen));
    }

    _last_seen = time(nullptr);
  }

  // return rc unchanged
  return rc;
}

bool Base::missing() const { return (!available()); }

bool Base::notValid() const { return !valid(); }

void Base::readFailure() { _read_errors++; }

void Base::readStart() { _read_us.reset(); }

uint64_t Base::readStop() {
  _read_us.freeze();
  _read_timestamp = time(nullptr);

  return (uint64_t)_read_us;
}

void Base::setID(const char *format, ...) {
  va_list arglist;

  va_start(arglist, format);
  _id.printf(format, arglist);
  va_end(arglist);
}

void Base::writeFailure() { _write_errors++; }

void Base::writeStart() { _write_us.reset(); }

uint64_t Base::writeStop() {
  _write_us.freeze();

  return (uint64_t)_write_us;
}

bool Base::valid() const { return (singleByteAddress() == 0x00 ? false : true); }
} // namespace device
