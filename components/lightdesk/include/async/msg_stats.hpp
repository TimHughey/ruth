//  Ruth
//  Copyright (C) 2022  Tim Hughey
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

#include "async/msg.hpp"
#include "async/msg_keys.hpp"
#include "io/io.hpp"
#include "ru_base/types.hpp"

#include <ArduinoJson.h>
#include <arpa/inet.h>
#include <concepts>
#include <esp_log.h>
#include <memory>

namespace ruth {
namespace desk {

class MsgStats : public Msg<asio::streambuf> {
public:
  inline MsgStats(asio::streambuf &packed) noexcept : Msg(packed), type(desk::STATS) {}

  inline ~MsgStats() noexcept {}           // prevent default copy/move
  inline MsgStats(MsgStats &&m) = default; // allow move
  inline MsgStats &operator=(MsgStats &&m) = default;

  template <typename T> inline void add_kv(JsonDocument &doc, csv key, T val) {

    if constexpr (std::is_same_v<T, Elapsed>) {
      doc[key] = val().count();
    } else if constexpr (std::same_as<T, Micros> || std::same_as<T, Millis>) {
      doc[key] = val.count();
    } else {
      doc[key] = val;
    }
  }

  inline auto serialize(JsonDocument &doc) noexcept {
    doc[MSG_TYPE] = type;
    doc[NOW_US] = rut::now_epoch<Micros>().count();
    doc[MAGIC] = MAGIC_VAL; // add magic as final key (to confirm complete msg)

    packed_len = measureMsgPack(doc);
    uint16_t msg_len = htons(packed_len);
    char *p = reinterpret_cast<char *>(&msg_len);

    auto hdr_buff = packed.prepare(hdr_bytes);
    auto *pbuff = static_cast<char *>(hdr_buff.data());

    pbuff[0] = *p++;
    pbuff[1] = *p;

    packed.commit(hdr_bytes);

    auto packed_buff = packed.prepare(packed_len);

    // NOTE:  reserve the first two bytes for the message length
    serializeMsgPack(doc, static_cast<char *>(packed_buff.data()), packed_buff.size());
    packed.commit(packed_len);
  }

  inline auto &write_buff() noexcept { return packed; }

  // order dependent
  csv type;

public:
  static constexpr const auto TAG{"desk::msg_out"};
};

} // namespace desk
} // namespace ruth