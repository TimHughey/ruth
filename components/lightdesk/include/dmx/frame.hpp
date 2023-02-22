//  Ruth
//  Copyright (C) 2020  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "ru_base/types.hpp"

#include "ArduinoJson.h"
#include <array>
#include <freertos/message_buffer.h>
#include <memory>

namespace ruth {

namespace dmx {
constexpr size_t FRAME_LEN{412}; // minimum to prevent flicker
using frame_data = std::array<uint8_t, FRAME_LEN>;

class frame : public dmx::frame_data {
public:
  static constexpr auto TAG{"dmx::frame"};

public:
  inline frame(size_t len = FRAME_LEN) noexcept : dmx::frame_data{0}, len(len) {}
  inline frame(JsonArrayConst array) noexcept : len{0} {

    if (array) {
      for (JsonVariantConst fb : array) {
        this->at(len) = fb.as<uint8_t>();
        len++;
      }
    } else {
      ESP_LOGW(TAG, "empty array");
    }
  }

  // uses std::array, prevent implict copy construction, move
  inline ~frame() noexcept {}

  inline frame(frame &&src) = default;            // allow move construction
  inline frame &operator=(frame &&rhs) = default; // allow move assignment

  size_t len;
};

} // namespace dmx
} // namespace ruth