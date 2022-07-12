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

#pragma once

#include "ArduinoJson.h"
#include "dev_ds/ds.hpp"
#include "message/in.hpp"

#include <memory>

namespace ruth {
namespace ds {
class DS2408 : public Device {

public:
  DS2408(const uint8_t *addr);

  bool execute(message::InWrapped msg) override;
  bool report() override;

  static constexpr size_t num_pins = 8;

private:
  bool cmdToMaskAndState(uint8_t pin, const char *cmd, uint8_t &mask, uint8_t &state);
  bool setPin(uint8_t pin, const char *cmd);
  bool status(uint8_t &states, uint64_t *elapsed_us = nullptr);
};

} // namespace ds
} // namespace ruth
