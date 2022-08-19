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
#include <array>
#include <freertos/message_buffer.h>
#include <memory>

#include "ru_base/types.hpp"

namespace ruth {

namespace dmx {
constexpr size_t FRAME_LEN = 384;
using frame_data = std::array<uint8_t, FRAME_LEN>;

class frame : public dmx::frame_data {
public:
  static constexpr csv TAG{"dmx::frame"};
  static constexpr size_t FRAME_LEN = 384; // minimum to prevent flicker

public:
  using frame_data::frame_data;
  inline frame(size_t len = FRAME_LEN) : dmx::frame_data{0}, len(len) {}
  inline frame(JsonArrayConst array) {

    len = 0;
    for (JsonVariantConst fb : array) {
      this->at(len) = fb.as<uint8_t>();
      len++;
    }
  }

  frame(frame &src) = delete;                     // no copy assignment
  frame(const frame &src) = delete;               // no copy constructor
  frame &operator=(frame &rhs) = delete;          // no copy assignment
  frame &operator=(const frame &rhs) = delete;    // no copy assignment
  inline frame(frame &&src) = default;            // allow move assignment
  inline frame &operator=(frame &&rhs) = default; // allow copy assignment

  size_t len;
};

} // namespace dmx
} // namespace ruth