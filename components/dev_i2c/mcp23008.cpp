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
static uint8_t _tx[11];

IRAM_ATTR MCP23008::MCP23008(uint8_t addr) : Device(addr, dev_description, MUTABLE) {}

IRAM_ATTR bool MCP23008::cmdToMaskAndState(uint8_t pin, const char *cmd, uint8_t &mask, uint8_t &state) {
  // guard against empty cmd or pin
  if (cmd == nullptr) return false;

  mask = 0x01 << pin;

  auto rc = false;

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
  bzero(_tx, sizeof(_tx));
  auto cmd = Bus::createCmd();

  i2c_master_write_byte(cmd, writeAddr(), true);
  i2c_master_write_byte(cmd, 0x00, true);

  i2c_master_write(cmd, _tx, sizeof(_tx), true); // send buffer with ACKs
  i2c_master_stop(cmd);

  auto rc = Bus::executeCmd(cmd);

  return rc;
}

IRAM_ATTR bool MCP23008::execute(message::InWrapped msg) {
  auto execute_rc = true;

  if (msg->unpack(cmd_doc)) {
    const char *refid = msg->refidFromFilter();
    message::Ack ack_msg(refid); // create ack msg early to capture execute us

    const JsonObject root = cmd_doc.as<JsonObject>();
    const char *cmd = root["cmd"];
    const uint8_t pin = root["pin"];

    execute_rc = setPin(pin, cmd);

    const bool ack = root["ack"] | true;

    if (ack && execute_rc) ruth::MQTT::send(ack_msg);
  }

  return execute_rc;
}

IRAM_ATTR bool MCP23008::refreshStates(int64_t *elapsed_us) {

  // register       register      register          register
  // 0x00 - IODIR   0x01 - IPOL   0x02 - GPINTEN    0x03 - DEFVAL
  // 0x04 - INTCON  0x05 - IOCON  0x06 - GPPU       0x07 - INTF
  // 0x08 - INTCAP  0x09 - GPIO   0x0a - OLAT

  // at POR the MCP2x008 operates in sequential mode where continued reads
  // automatically increment the address (register).  we read all registers
  // (11 bytes) in one shot.

  auto start_at = esp_timer_get_time();
  auto cmd = Bus::createCmd();

  // establish comms with the device by sending it's address, with write flag, followed by a single
  // control register address byte.  the device will ACK each byte.
  i2c_master_write_byte(cmd, writeAddr(), true);
  i2c_master_write_byte(cmd, 0x00, true);

  // now send a START and device address with the read flag
  // read 11 bytes of control register data.
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, readAddr(), true);
  i2c_master_read(cmd, _rx, sizeof(_rx), I2C_MASTER_LAST_NACK);
  i2c_master_stop(cmd);

  auto rc = Bus::executeCmd(cmd);
  statesStore(rc, _rx[0x09]);

  if (elapsed_us) *elapsed_us = _states_at - start_at;

  return rc;
}

IRAM_ATTR bool MCP23008::report(const bool send) {
  message::States states_rpt(_ident);

  refreshStates();

  auto rc = true;
  uint8_t states_raw;
  if (states(states_raw)) {

    for (size_t i = 0; i < num_pins; i++) {
      const char *state = (states_raw & (0x01 << i)) ? "on" : "off";

      states_rpt.addPin(i, state);
    }
  } else {
    rc = false;
    states_rpt.setError();
  }

  states_rpt.finalize();

  if (send) ruth::MQTT::send(states_rpt);

  return rc;
}

IRAM_ATTR bool MCP23008::setPin(uint8_t pin, const char *cmd) {
  uint8_t have_states;

  // NOTE: check has side-effect of rejecting cmds until first report
  if (states(have_states) == false) {
    return false;
  }

  auto rc = false;
  uint8_t cmd_state;
  uint8_t cmd_mask;

  if (cmdToMaskAndState(pin, cmd, cmd_mask, cmd_state)) {
    // XOR the new state against the as_is state using the mask to only change the pin specified
    // by the command
    const uint8_t next_states = have_states ^ ((have_states ^ cmd_state) & cmd_mask);

    ESP_LOGD(_ident, "have_states[%02x] next_states[%02x]", have_states, next_states);

    bzero(_tx, sizeof(_tx));
    _tx[0x0a] = next_states;

    auto cmd = Bus::createCmd();
    i2c_master_write_byte(cmd, writeAddr(), true);
    i2c_master_write_byte(cmd, 0x00, true);

    i2c_master_write(cmd, _tx, sizeof(_tx), true);
    i2c_master_stop(cmd);

    rc = Bus::executeCmd(cmd);
    statesStore(rc, next_states);
  }

  return rc;
}

IRAM_ATTR void MCP23008::setupTxBuffer() {
  bzero(_tx, sizeof(_tx));
  _tx[0] = writeAddr(); // start comms by addressing the device in write mode
  _tx[1] = 0x00;        // start writing at the first byte of the control register
}

bool MCP23008::statesStore(const bool rc, const uint8_t states) {

  if (!rc) ESP_LOGW(_ident, "%s %s", __PRETTY_FUNCTION__, (rc) ? "true" : "false");

  if (rc) {
    _states_at = esp_timer_get_time();
    _states = states;
    _states_rc = rc;
  }

  return rc;
}

} // namespace i2c
