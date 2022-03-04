/*
    i2c/mcp23008.hpp - Ruth I2C MCP23008 Device
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

#ifndef ruth_dev_mcp23008_hpp
#define ruth_dev_mcp23008_hpp

#include "dev_i2c/i2c.hpp"

namespace i2c {

class MCP23008 : public Device {
public:
  MCP23008(uint8_t addr = 0x20);

  bool execute(message::InWrapped msg) override;
  bool report() override;

public:
  static constexpr size_t num_pins = 8;

private:
  bool cmdToMaskAndState(uint8_t pin, const char *cmd, uint8_t &mask, uint8_t &state);
  bool refreshStates(int64_t *elapsed_us = nullptr);
  bool setPin(uint8_t pin, const char *cmd);

  inline bool states(uint8_t &states) const {
    states = _states;
    return _states_rc;
  };

  inline bool statesStore(const bool rc, const uint8_t states) {
    if (rc) {
      _states = states;
      _states_rc = rc;
    }

    return rc;
  }

private:
  uint8_t _states = 0x00;
  bool _states_rc = false;
};

} // namespace i2c

#endif
