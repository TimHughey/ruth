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

#include <esp_log.h>

#include "crc.hpp"
#include "dev_ds/ds2408.hpp"

namespace ds {

DS2408::DS2408(const uint8_t *addr) : Device(addr) { _mutable = true; }

IRAM_ATTR bool DS2408::execute() { return true; }

IRAM_ATTR bool DS2408::report() {
  uint8_t states;

  auto rc = status(states);

  return rc;
}

IRAM_ATTR bool DS2408::status(uint8_t &states, uint64_t *elapsed_us) {
  // status (state of pins) of a DS2408 device
  // one contiguous array of bytes to:
  //  1. match the rom and initiate channel access read mode
  //  2. receive 32 bytes of channel access data from the device
  //
  // this is one contiguous array because the DS2408 sends an inverted CRC as the
  // final two bytes that includes the last byte of the channel access commmand.  by
  // making this one contiguous buffer we can use pointer math to compute the crc16.

  static uint8_t read_cmd[] = {0x55,                   // byte 0: match ROM
                               0x00, 0x00, 0x00, 0x00, // byte 1-4: rom bytes
                               0x00, 0x00, 0x00, 0x00, // byte 1-8: rom bytes
                               0xf5,                   // byte 9: channel access read cmd
                               0x00, 0x00, 0x00, 0x00, // bytes 10-41: channel state data
                               0x00, 0x00, 0x00, 0x00, // channel state data
                               0x00, 0x00, 0x00, 0x00, // channel state data
                               0x00, 0x00, 0x00, 0x00, // channel state data
                               0x00, 0x00, 0x00, 0x00, // channel state data
                               0x00, 0x00, 0x00, 0x00, // channel state data
                               0x00, 0x00, 0x00, 0x00, // channel state data
                               0x00, 0x00, 0x00, 0x00, // channel state data
                               0x00, 0x00};            // bytes 42-43: CRC16

  auto rc = false;
  auto start_at = now();

  static uint8_t *cmd = read_cmd;
  static constexpr size_t cmd_len = 10;
  static uint8_t *data = read_cmd + cmd_len;
  static constexpr size_t data_len = sizeof(read_cmd) - cmd_len;

  if (matchRomThenRead(cmd, cmd_len, data, data_len) == false) return false;
  // calculate the start/end of the block for the crc16 validation
  const uint8_t *crc_start = data - 1; // include the channel access read byte 0xf5
  const uint8_t *crc_end = data + data_len;

  if (checkCrc16(crc_start, crc_end)) {
    rc = true; // success

    // invert states; device considers on as false, off as true
    states = ~(data[31]) & 0xff; // constrain to 8bits

    if (elapsed_us) *elapsed_us = now() - start_at;

    ESP_LOGD(ident(), "states: 0x%02x", states);
  }

  return rc;
}

// bool DallasSemi::setDS2408(DsDevice_t *dev, uint32_t cmd_mask,
//                            uint32_t cmd_state) {
//   owb_status owb_s;
//   bool rc = false;
//
//   // read the device to ensure we have the current state
//   // important because setting the new state relies, in part, on the existing
//   // state for the pios not changing
//   if (readDevice(dev) == false) {
//     Text::rlog("device \"%s\" read before set failed", dev->id());
//
//     return rc;
//   }
//
//   Positions_t *reading = (Positions_t *)dev->reading();
//
//   uint32_t asis_state = reading->state();
//   uint32_t new_state = 0x00;
//
//   // use XOR tricks to apply the state changes to the as_is state using the
//   // mask computed
//   new_state = asis_state ^ ((asis_state ^ cmd_state) & cmd_mask);
//
//   // report_state = new_state;
//   new_state = (~new_state) & 0xFF; // constrain to 8-bits
//
//   resetBus();
//
//   uint8_t dev_cmd[] = {0x55, // byte 0: match rom_code
//                        0x00, // byte 1-8: rom
//                        0x00,
//                        0x00,
//                        0x00,
//                        0x00,
//                        0x00,
//                        0x00,
//                        0x00,
//                        0x5a,                 // byte 9: actual device cmd
//                        (uint8_t)new_state,   // byte10 : new state
//                        (uint8_t)~new_state}; // byte11: inverted state
//
//   dev->copyAddrToCmd(dev_cmd);
//   owb_s = owb_write_bytes(_ds, dev_cmd, sizeof(dev_cmd));
//
//   if (owb_s != OWB_STATUS_OK) {
//     Text::rlog("device \"%s\" set cmd failed owb_s=\"%d\"", dev->debug().get(),
//                owb_s);
//
//     return rc;
//   }
//
//   uint8_t check[2];
//   // read the confirmation byte (0xAA) and new state
//   owb_s = owb_read_bytes(_ds, check, sizeof(check));
//
//   if (owb_s != OWB_STATUS_OK) {
//
//     Text::rlog("device \"%s\" check byte read failed owb_s=\"%d\"",
//                dev->debug().get(), owb_s);
//
//     return rc;
//   }
//
//   resetBus();
//
//   // check what the device returned to determine success or failure
//   // byte 0: 0xAA is the confirmation response
//   // byte 1: new_state
//   uint8_t conf_byte = check[0];
//   uint8_t dev_state = check[1];
//   if ((conf_byte == 0xaa) || (dev_state == new_state)) {
//     rc = true;
//   } else if (((conf_byte & 0xa0) == 0xa0) || ((conf_byte & 0x0a) == 0x0a)) {
//     Text::rlog("device \"%s\" SET OK-PARTIAL conf=\"%02x\" state "
//                "req=\"%02x\" dev=\"%02x\"",
//                dev->id(), conf_byte, new_state, dev_state);
//     rc = true;
//   } else {
//     Text::rlog("device \"%s\" SET FAILED conf=\"%02x\" state req=\"%02x\" "
//                "dev=\"%02x\"",
//                dev->id(), conf_byte, new_state, dev_state);
//   }
//
//   return rc;
// }

} // namespace ds
