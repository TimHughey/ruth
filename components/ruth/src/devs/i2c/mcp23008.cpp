/*
    i2c/mcp23008.cpp - Ruth I2C Device MCP23008
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

#include <driver/i2c.h>
#include <esp_log.h>

#include "devs/base/base.hpp"
#include "devs/i2c/mcp23008.hpp"

namespace ruth {

MCP23008::MCP23008(uint8_t bus, uint8_t addr) : I2cDevice(addr, bus) {}

MCP23008::MCP23008(const MCP23008_t &rhs, time_t missing_secs)
    : I2cDevice(rhs.address(), rhs.bus(), missing_secs) {
  // only make the device ID when a copy is made from a device reference
  makeID();
}

bool MCP23008::detect() {
  clearPreviousError();

  _tx = {0x00}; // IODIR Register (address 0x00)
  _rx.reserve(11);

  // at POR the MCP2x008 operates in sequential mode where continued reads
  // automatically increment the address (register).  we read all registers
  // (12 bytes) in one shot.

  return requestData(_tx, _rx);
}

bool MCP23008::read() {
  auto rc = false;
  auto positions = 0b00000000;

  clearPreviousError();

  _tx = {0x00}; // IODIR Register (address 0x00)

  // register       register      register          register
  // 0x00 - IODIR   0x01 - IPOL   0x02 - GPINTEN    0x03 - DEFVAL
  // 0x04 - INTCON  0x05 - IOCON  0x06 - GPPU       0x07 - INTF
  // 0x08 - INTCAP  0x09 - GPIO   0x0a - OLAT

  // at POR the MCP2x008 operates in sequential mode where continued reads
  // automatically increment the address (register).  we read all registers
  // (11 bytes) in one shot.
  _rx.reserve(11);

  if (requestData(_tx, _rx)) {
    // GPIO register is little endian, conversion is required
    positions = _rx[0x0a]; // OLAT register (address 0x0a)

    justSeen();

    Positions_t *reading = new Positions(id(), positions, (uint8_t)8);

    reading->setLogReading();
    setReading(reading);
    rc = true;
  } else {
    readFailure();
  }

  return rc;
}

bool MCP23008::writeState(uint32_t cmd_mask, uint32_t cmd_state) {
  clearPreviousError();

  // read the device to ensure we have the current state
  // important because setting the new state relies, in part, on the
  // existing state for the pios not changing
  if (read() == false) {
    Text::rlog("device \"%s\" read before set failed", id());

    return false;
  }

  writeStart();

  // if register 0x00 (IODIR) is not 0x00 (IODIR isn't output) then
  // set it to output
  if (_rx.at(0) > 0x00) {
    _tx = {0x00, 0x00};
    _rx.clear();

    if (requestData(_tx, _rx) == false) {
      writeStop();
      return false;
    }
  }

  auto *reading = (Positions_t *)this->reading();
  auto asis_state = reading->state();
  auto new_state = 0x00;

  // XOR the new state against the as_is state using the mask
  // it is critical that we use the recently read state to avoid
  // overwriting the device state that MCP is not aware of
  new_state = asis_state ^ ((asis_state ^ cmd_state) & cmd_mask);

  // to set the GPIO we will write to two registers:
  // a. IODIR (0x00) - setting all GPIOs to output (0b00000000)
  // b. OLAT (0x0a)  - the new state
  _tx = {0x0a, (uint8_t)(new_state & 0xff)};
  _rx.clear();

  auto rc = false;
  if ((rc = requestData(_tx, _rx)) == false) {
    Text::rlog("[%s] device \"%s\" set failed", recentError(), id());
  }

  writeStop();

  return rc;
}

} // namespace ruth
