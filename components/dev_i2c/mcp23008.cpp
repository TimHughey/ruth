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
#include <esp_attr.h>
#include <esp_log.h>

#include "ArduinoJson.h"
#include "bus.hpp"
#include "dev_i2c/i2c.hpp"
#include "dev_i2c/mcp23008.hpp"
#include "message/ack_msg.hpp"
#include "message/states_msg.hpp"
#include "ruth_mqtt/mqtt.hpp"

namespace i2c {
// static const char *TAG = "i2c::mcp23008";
static const char *dev_description = "mcp23008";
static StaticJsonDocument<1024> cmd_doc;
static uint8_t _rx[11];
static uint8_t _tx[13];

IRAM_ATTR MCP23008::MCP23008(uint8_t addr) : Device(addr, dev_description, MUTABLE) {}

IRAM_ATTR bool MCP23008::cmdToMaskAndState(uint8_t pin, const char *cmd, uint8_t &mask, uint8_t &state) {
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

  ESP_LOGD(_ident, "pin[%u] cmd[%s] mask[%02x] state[%02x]", pin, cmd, mask, state);

  return rc;
}

IRAM_ATTR bool MCP23008::detect() {
  // to detect the MCP23008 we send the desired configuration and check for success.  noting the
  // MCP23008 is in sequential byte mode at startup so we can send all bytes in one shot.

  // set the control register values zeroes.  this will ensure the IODIR is output and the
  // GPIO pins are logic low.
  setupTxBuffer();
  auto cmd = Bus::createCmd();
  i2c_master_write(cmd, _tx, sizeof(_tx), true); // send buffer with ACKs
  i2c_master_stop(cmd);

  auto rc = Bus::executeCmd(cmd);

  if (rc) updateSeenTimestamp();

  return rc;
}

IRAM_ATTR bool MCP23008::execute(message::InWrapped msg) {
  auto execute_rc = true;

  if (msg->unpack(cmd_doc)) {
    const char *refid = msg->filterExtra(1);
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

IRAM_ATTR bool MCP23008::report(const bool send) {
  message::States states(_ident);
  uint8_t states_raw;
  auto rc = status(states_raw);

  if (rc) {
    updateSeenTimestamp();

    for (size_t i = 0; i < num_pins; i++) {
      const char *state = (states_raw & (0x01 << i)) ? "on" : "off";

      states.addPin(i, state);
    }
  } else {
    states.setError();
  }

  states.finalize();

  if (send) ruth::MQTT::send(states);

  return rc;
}

IRAM_ATTR bool MCP23008::setPin(uint8_t pin, const char *cmd) {
  auto rc = false;

  uint8_t asis_states;
  if (status(asis_states)) {
    uint8_t cmd_state;
    uint8_t cmd_mask;

    if (cmdToMaskAndState(pin, cmd, cmd_mask, cmd_state)) {
      // XOR the new state against the as_is state using the mask to only change the pin specified
      // by the command
      const uint8_t new_states = asis_states ^ ((asis_states ^ cmd_state) & cmd_mask);

      ESP_LOGD(_ident, "asis_states[%02x] new_states[%02x]", asis_states, new_states);

      setupTxBuffer();
      _tx[0x0a + 2] = new_states;

      auto cmd = Bus::createCmd();
      i2c_master_write(cmd, _tx, sizeof(_tx), true);
      i2c_master_stop(cmd);

      rc = Bus::executeCmd(cmd);
    }
  }

  return rc;
}

IRAM_ATTR void MCP23008::setupTxBuffer() {
  bzero(_tx, sizeof(_tx));
  _tx[0] = writeAddr(); // start comms by addressing the device in write mode
  _tx[1] = 0x00;        // start writing at the first byte of the control register
}

IRAM_ATTR bool MCP23008::status(uint8_t &states, uint64_t *elapsed_us) {
  auto rc = false;

  // register       register      register          register
  // 0x00 - IODIR   0x01 - IPOL   0x02 - GPINTEN    0x03 - DEFVAL
  // 0x04 - INTCON  0x05 - IOCON  0x06 - GPPU       0x07 - INTF
  // 0x08 - INTCAP  0x09 - GPIO   0x0a - OLAT

  // at POR the MCP2x008 operates in sequential mode where continued reads
  // automatically increment the address (register).  we read all registers
  // (11 bytes) in one shot.

  auto start_at = now();
  auto cmd = Bus::createCmd();

  // establish comms with the device by sending it's address, with write flag, followed by a single
  // control register address byte.  the device will ACK each byte.
  i2c_master_write_byte(cmd, writeAddr(), true);
  i2c_master_write_byte(cmd, 0x00, true);

  // now send a START followed the device address with the read flag then read the 11 bytes of
  // control register data. the MCP23008 spec does not describe
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, readAddr(), true);
  i2c_master_read(cmd, _rx, sizeof(_rx), I2C_MASTER_LAST_NACK);
  i2c_master_stop(cmd);

  rc = Bus::executeCmd(cmd);

  if (rc) {
    states = _rx[0x09];

    if (elapsed_us) *elapsed_us = now() - start_at;
  }

  return rc;
}

} // namespace i2c
