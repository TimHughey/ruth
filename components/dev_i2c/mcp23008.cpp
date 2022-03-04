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

static const char *cmd_text[] = {"on", "off"};
constexpr size_t ON = 0;
constexpr size_t OFF = 1;

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
  static auto once = true;

  // register       register      register          register
  // 0x00 - IODIR   0x01 - IPOL   0x02 - GPINTEN    0x03 - DEFVAL
  // 0x04 - INTCON  0x05 - IOCON  0x06 - GPPU       0x07 - INTF
  // 0x08 - INTCAP  0x09 - GPIO   0x0a - OLAT

  if (once) {
    constexpr size_t bytes = 11;
    constexpr uint8_t iocon = 0x05;
    constexpr uint8_t seqop_disable = 0x20;
    uint8_t tx[bytes] = {};

    auto cmd = Bus::createCmd();
    tx[iocon] = seqop_disable; // disable sequential byte mode

    // address the device for write
    i2c_master_write_byte(cmd, writeAddr(), I2C_MASTER_ACK);
    // MCP23008 defaults to SEQOP at POR, write the entire byte stream
    i2c_master_write(cmd, tx, bytes, I2C_MASTER_ACK);
    i2c_master_stop(cmd);

    Bus::executeCmd(cmd);

    once = false;
  }

  constexpr uint8_t gpio_reg = 0x09;
  auto cmd = Bus::createCmd();

  // send a START and device address with the read flag
  // read 11 bytes of control register data.
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, writeAddr(), I2C_MASTER_ACK); // address the MCP23008
  i2c_master_write_byte(cmd, gpio_reg, I2C_MASTER_ACK);    // write the register to read
  i2c_master_start(cmd);                                   // restart
  i2c_master_write_byte(cmd, readAddr(), I2C_MASTER_ACK);  // signal to read
  uint8_t gpio_port_val = 0x00;
  i2c_master_read_byte(cmd, &gpio_port_val, I2C_MASTER_LAST_NACK); // read the register
  i2c_master_stop(cmd);

  auto rc = Bus::executeCmd(cmd);

  ESP_LOGD(_ident, "gpio_port 0x%02x %s", gpio_port_val, (rc) ? "true" : "false");
  statesStore(rc, gpio_port_val);

  return rc;
}

IRAM_ATTR bool MCP23008::report() {
  refreshStates();

  uint8_t states_raw;
  if (states(states_raw)) {
    message::States states_rpt(_ident);

    for (size_t i = 0; i < num_pins; i++) {
      const char *state = (states_raw & (0x01 << i)) ? cmd_text[ON] : cmd_text[OFF];

      states_rpt.addPin(i, state);
    }

    states_rpt.finalize();

    ruth::MQTT::send(states_rpt);
    return true;
  }

  return false;
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
    constexpr uint8_t olat_reg = 0x0a;
    // next olat is the XOR of have_state and mask and cmd pin
    const uint8_t olat_val = have_states ^ ((have_states ^ cmd_state) & cmd_mask);

    ESP_LOGD(_ident, "have_states[%02x] olat[%02x]", have_states, olat_val);

    auto cmd = Bus::createCmd();
    i2c_master_write_byte(cmd, writeAddr(), I2C_MASTER_ACK);
    i2c_master_write_byte(cmd, olat_reg, I2C_MASTER_ACK);
    i2c_master_write_byte(cmd, olat_val, I2C_MASTER_ACK);
    i2c_master_stop(cmd);

    rc = Bus::executeCmd(cmd);
    statesStore(rc, olat_val);
  }

  return rc;
}

} // namespace i2c
