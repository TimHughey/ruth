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

#include "dev_ds/ds2408.hpp"
#include "ArduinoJson.h"
#include "dev_ds/crc.hpp"
#include "message/ack_msg.hpp"
#include "message/states_msg.hpp"
#include "ruth_mqtt/mqtt.hpp"

#include <esp_log.h>
#include <esp_timer.h>

namespace ruth {

namespace ds {

DS2408::DS2408(const uint8_t *addr) : Device(addr) { _mutable = true; }

static StaticJsonDocument<1024> cmd_doc;

IRAM_ATTR bool DS2408::execute(message::InWrapped msg) {
  auto execute_rc = true;

  if (msg->unpack(cmd_doc)) {
    const char *refid = msg->refidFromFilter();
    const JsonObject root = cmd_doc.as<JsonObject>();

    const char *cmd = root["cmd"];
    const uint8_t pin = root["pin"];

    execute_rc = setPin(pin, cmd);

    const bool ack = root["ack"] | false;

    if (ack && execute_rc) {
      updateSeenTimestamp();
      message::Ack ack_msg(refid);

      ruth::MQTT::send(ack_msg);
    }
  }

  return execute_rc;
}

IRAM_ATTR bool DS2408::report() {

  message::States states(ident());
  uint8_t states_raw;
  auto rc = status(states_raw);

  if (rc) {
    updateSeenTimestamp();

    for (auto i = 0; i < num_pins; i++) {
      const char *state = (states_raw & (0x01 << i)) ? "on" : "off";

      states.addPin(i, state);
    }
  } else {
    states.setError();
  }

  states.finalize();

  ruth::MQTT::send(states);

  return rc;
}

IRAM_ATTR bool DS2408::setPin(uint8_t pin, const char *cmd) {
  auto rc = false;

  uint8_t asis_states;
  if (status(asis_states)) {
    uint8_t cmd_state;
    uint8_t cmd_mask;

    if (cmdToMaskAndState(pin, cmd, cmd_mask, cmd_state)) {
      const uint8_t new_states = ~(asis_states ^ ((asis_states ^ cmd_state) & cmd_mask));

      ESP_LOGD(ident(), "asis_states[%02x] new_states[%02x]", asis_states, new_states);

      static uint8_t set_pin_cmd[12];
      static constexpr size_t pin_cmd_len = sizeof(set_pin_cmd);
      set_pin_cmd[9] = 0x5a; // set command
      set_pin_cmd[10] = new_states;
      set_pin_cmd[11] = ~new_states;

      static uint8_t check[2];
      static constexpr size_t check_len = sizeof(check);

      if (matchRomThenRead(set_pin_cmd, pin_cmd_len, check, check_len)) {
        // check what the device returned to determine success or failure
        // byte 0: 0xAA is the confirmation response
        // byte 1: new_state
        uint8_t conf_byte = check[0];
        uint8_t dev_state = check[1];
        if ((conf_byte == 0xaa) || (dev_state == new_states)) {
          rc = true;
        } else if (((conf_byte & 0xa0) == 0xa0) || ((conf_byte & 0x0a) == 0x0a)) {
          ESP_LOGW(ident(), "SET OK-PARTIAL conf[%02x] req[%02x] dev[%02x]", conf_byte, new_states,
                   dev_state);
          rc = true;
        } else {
          ESP_LOGW(ident(), "SET FAILED conf[%02x] req[%02x] dev[%02x]", conf_byte, new_states,
                   dev_state);
        }
      }
    }
  }

  return rc;
}

IRAM_ATTR bool DS2408::cmdToMaskAndState(uint8_t pin, const char *cmd, uint8_t &mask,
                                         uint8_t &state) {

  auto rc = false;

  mask = 0x01 << pin;

  if (cmd[0] == 'o') {
    if ((cmd[1] == 'f') && (cmd[2] == 'f')) {
      rc = true;
      state = 0x00;
    }

    if (cmd[1] == 'n') {
      rc = true;
      state = 0x01 << pin;
    }
  }

  ESP_LOGD(ident(), "pin[%u] cmd[%s] mask[%02x] state[%02x]", pin, cmd, mask, state);

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
  auto start_at = esp_timer_get_time();

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

    if (elapsed_us) *elapsed_us = esp_timer_get_time() - start_at;

    ESP_LOGD(ident(), "states: 0x%02x", states);
  }

  return rc;
}

} // namespace ds
} // namespace ruth
