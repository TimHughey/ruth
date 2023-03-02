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
#include "misc/elapsed.hpp"
#include "ru_base/rut.hpp"
#include "ru_base/types.hpp"

#include <ArduinoJson.h>
#include <arpa/inet.h>
#include <array>
#include <concepts>
#include <esp_log.h>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace ruth {
namespace desk {

class MsgOut : public Msg<packed_out_t> {
public:
  inline MsgOut(csv type, JsonDocument &doc, packed_out_t &packed) noexcept
      : Msg(packed), //
        type(type),  //
        doc_ref(doc) //
  {
    doc[TYPE] = type;
  }

  inline ~MsgOut() noexcept {}         // prevent default copy/move
  inline MsgOut(MsgOut &&m) = default; // allow move
  inline MsgOut &operator=(MsgOut &&m) = default;

  template <typename T> inline void add_kv(csv key, T val) {
    auto &doc = doc_ref.get();

    if constexpr (std::is_same_v<T, Elapsed>) {
      doc[key] = val().count();
    } else if constexpr (std::same_as<T, Micros> || std::same_as<T, Millis>) {
      doc[key] = val.count();
    } else {
      doc[key] = val;
    }
  }

  inline auto serialize() noexcept {
    auto &doc = doc_ref.get();

    doc[NOW_US] = rut::now_epoch<Micros>().count();
    doc[MAGIC] = MAGIC_VAL; // add magic as final key (to confirm complete msg)

    packed_len = measureMsgPack(doc);
    packed.assign(packed_len + hdr_bytes, 0x00);

    // NOTE:  reserve the first two bytes for the message length
    serializeMsgPack(doc, packed.data() + hdr_bytes, packed.size() - hdr_bytes);

    uint16_t msg_len = htons(packed_len);
    char *p = reinterpret_cast<char *>(&msg_len);

    packed[0] = *p++;
    packed[1] = *p;
  }

  inline auto write_buff() noexcept { return asio::buffer(packed); }

  // order dependent
  csv type;
  std::reference_wrapper<JsonDocument> doc_ref;

public:
  static constexpr const auto TAG{"desk::msg_out"};
};

} // namespace desk
} // namespace ruth