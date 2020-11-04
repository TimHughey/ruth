/*
    sht31.cpp - Ruth I2C Device MCP23008
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

#include <memory>
#include <string>

#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <sys/time.h>
#include <time.h>

#include "devs/base/base.hpp"
#include "devs/i2c/mcp23008.hpp"
#include "local/types.hpp"
#include "net/network.hpp"

namespace ruth {

MCP23008::MCP23008(uint8_t bus) : I2cDevice(0x20, bus) {}

bool MCP23008::detectOnBus() {
  auto rc = false;

  _esp_rc_prev = ESP_OK;

  _tx = {0x30, 0xa2};
  _rx.clear();

  requestData();

  if (_esp_rc == ESP_OK) {
    rc = true;
    detected();
  }

  return rc;
}

bool MCP23008::read() {
  auto rc = false;
  auto positions = 0b00000000;
  esp_err_t esp_rc;

  _tx = {0x00}; // IODIR Register (address 0x00)

  // register       register      register          register
  // 0x00 - IODIR   0x01 - IPOL   0x02 - GPINTEN    0x03 - DEFVAL
  // 0x04 - INTCON  0x05 - IOCON  0x06 - GPPU       0x07 - INTF
  // 0x08 - INTCAP  0x09 - GPIO   0x0a - OLAT

  // at POR the MCP2x008 operates in sequential mode where continued reads
  // automatically increment the address (register).  we read all registers
  // (12 bytes) in one shot.
  _rx.reserve(12);
  _rx.assign(12, 0x00);

  resetRequestSequence();
  esp_rc = requestData();

  if (esp_rc == ESP_OK) {
    // GPIO register is little endian so no conversion is required
    positions = _rx[0x0a]; // OLAT register (address 0x0a)

    storeRawData(_rx);

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

  resetRequestSequence();

  // read the device to ensure we have the current state
  // important because setting the new state relies, in part, on the
  // existing state for the pios not changing
  if (read() == false) {
    Text::rlog("device \"%s\" read before set failed", debug().get());

    return false;
  }

  // if register 0x00 (IODIR) is not 0x00 (IODIR isn't output) then
  // set it to output
  if (_rx.at(0) > 0x00) {
    _tx = {0x00, 0x00};
    if (requestData() != ESP_OK) {
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

  if (requestData() != ESP_OK) {
    Text::rlog("[%s] device \"%s\" set failed", esp_err_to_name(_esp_rc),
               debug().get());

    return false;
  }

  return true;
}

} // namespace ruth
