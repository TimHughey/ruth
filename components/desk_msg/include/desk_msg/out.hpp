
// Ruth
// Copyright (C) 2022  Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "desk_msg/kv.hpp"
#include "desk_msg/kv_store.hpp"
#include "desk_msg/msg.hpp"
#include "io/io.hpp"
#include "misc/elapsed.hpp"
#include "ru_base/rut.hpp"
#include "ru_base/types.hpp"

#include <ArduinoJson.h>
#include <concepts>
#include <esp_log.h>
#include <type_traits>

namespace ruth {
namespace desk {

class MsgOut : public Msg {

protected:
  inline auto *raw_out(auto &buffer) noexcept { return static_cast<char *>(buffer.data()); }

  virtual void serialize_hook(JsonDocument &) noexcept {}

public:
  // outbound messages
  inline MsgOut(const char *msg_type) noexcept : Msg(256), msg_type(msg_type) {}

  // inbound messages
  virtual ~MsgOut() noexcept {} // prevent implicit copy/move

  inline MsgOut(MsgOut &&m) = default;
  MsgOut &operator=(MsgOut &&) = default;

  inline void operator()(const error_code &op_ec, size_t n) noexcept {
    xfr.out += n;
    ec = op_ec;

    if (n == 0) ESP_LOGD(TAG, "SHORT WRITE n=%d err=%s\n", xfr.in, ec.message().c_str());
  }

  inline void operator()(kv_store &&kvs_extra) noexcept {
    kvs.add(std::forward<kv_store>(kvs_extra));
  }

  inline void commit(std::size_t n) noexcept { storage->commit(n); }

  template <typename Val> inline void add_kv(auto &&key, Val &&val) noexcept {
    if constexpr (std::same_as<Val, Elapsed>) {
      kvs.add(std::forward<decltype(key)>(key), val());
    } else if constexpr (std::same_as<Val, Micros> || std::same_as<Val, Millis>) {
      kvs.add(std::forward<decltype(key)>(key), val.count());
    } else {
      kvs.add(std::forward<decltype(key)>(key), std::forward<Val>(val));
    }
  }

  auto prepare() noexcept { return storage->prepare(storage->max_size()); }

  inline auto serialize() noexcept {
    StaticJsonDocument<Msg::default_doc_size> doc;

    // first, add MSG_TYPE as it is used to detect start of message
    doc[desk::MSG_TYPE] = msg_type;

    // allow subclasses to add special data directly to the doc
    serialize_hook(doc);

    // put added key/vals into the document
    kvs.populate_doc(doc);

    // include the elapsed time to prepare the buffer by doing so before
    // we finish populating the doc
    auto buffer = prepare();

    doc[MAGIC] = MAGIC_VAL; // add magic as final key (to confirm complete msg)

    packed_len = serializeMsgPack(doc, raw_out(buffer), buffer.size());

    commit(packed_len);

    ESP_LOGI(TAG, "serialized, packed_len=%u mem_used=%u", packed_len, doc.memoryUsage());
  }

public:
  // order dependent
  string msg_type;

  // order independent
  kv_store kvs;

public:
  static auto constexpr TAG{"desk.msg.out"};
};

} // namespace desk
} // namespace ruth