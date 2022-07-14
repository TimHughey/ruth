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

#include "base/uint8v.hpp"
#include "io/io.hpp"

#include "ArduinoJson.h"
#include <memory>

namespace ruth {

class DeskMsg;
typedef std::shared_ptr<DeskMsg> shDeskMsg;

class DeskMsg : public std::enable_shared_from_this<DeskMsg> {

private:
  static constexpr csv MAGIC{"magic"};
  static constexpr csv DFRAME{"dframe"};
  typedef DynamicJsonDocument MsgPackDoc;

public:
  DeskMsg(std::array<char, 1024> &buff, size_t capacity = 1024) : doc(capacity) {

    if (auto err = deserializeMsgPack(doc, buff.data()); !err) {
      deserialize_ok = true;
      root_obj = doc.as<JsonObjectConst>();
    }
  }

  inline auto root() const { return root_obj; }
  inline bool good() const { return deserialize_ok; }

public:
  template <typename T> inline T dframe() const {
    T dmx_f = T();

    for (JsonVariantConst frame_byte : root_obj[DFRAME].as<JsonArrayConst>()) {
      dmx_f.emplace_back(frame_byte.as<uint8_t>());
    }

    return std::move(dmx_f);
  }

  inline bool validMagic() const { return good() && ((root_obj[MAGIC] | 0x00) == 0xc9d2); }

private:
  uint8v rx_buff;
  MsgPackDoc doc;
  JsonObjectConst root_obj;

  bool deserialize_ok = false;
};

} // namespace ruth
