/*
    Ruth
    Copyright (C) 2021  Tim Hughey

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

#include "io/io.hpp"
#include "misc/elapsed.hpp"
#include "ru_base/time.hpp"
#include "ru_base/uint8v.hpp"

#include "ArduinoJson.h"
#include <array>
#include <esp_log.h>
#include <esp_timer.h>
#include <memory>
#include <optional>

namespace ruth {

class DeskMsg {

private:
  static constexpr csv TAG{"DeskMsg"};

  // doc keys
  static constexpr csv MAGIC{"magic"};
  static constexpr csv SEQ_NUM{"seq_num"};
  static constexpr csv DFRAME{"dframe"};

  // doc constant values
  static constexpr uint32_t MAGIC_VAL{0xc9d2};

public:
  inline DeskMsg(JsonDocument &doc) : root_obj(doc.as<JsonObjectConst>()) {}

  inline auto root() const { return root_obj; }

public:
  inline bool can_render() const { return root_obj[MAGIC].as<int64_t>() == MAGIC_VAL; }

  template <typename T> inline T dframe() const {

    if (auto array = root_obj[DFRAME].as<JsonArrayConst>(); array) {
      return T(array);
    }

    return T(0);
  }

private:
  // order dependent
  JsonObjectConst root_obj;
};

} // namespace ruth
