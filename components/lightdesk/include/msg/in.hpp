
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

#include "msg/msg.hpp"

#include <ArduinoJson.h>
#include <arpa/inet.h>
#include <esp_log.h>
#include <memory>

namespace ruth {
namespace desk {

class MsgIn : public Msg {
protected:
  inline auto *raw_in() noexcept { return static_cast<const char *>(storage->data().data()); }

public:
  inline MsgIn() : Msg(512) {}
  ~MsgIn() noexcept {}

  inline MsgIn(MsgIn &&) = default;
  MsgIn &operator=(MsgIn &&) = default;

  void operator()(const error_code &op_ec, size_t n) noexcept {
    static constexpr auto TAG{"desk.msgin.async_result"};

    xfr.in += n;
    ec = op_ec;
    packed_len = n; // should we need to set this?

    if (n == 0) {
      ESP_LOGD(TAG, "SHORT READ  n=%d err=%s\n", xfr.in, ec.message().c_str());
    }
  }

  inline static bool can_render(const JsonDocument &doc) noexcept {
    return doc[desk::MAGIC] == MAGIC_VAL;
  }

  inline auto deserialize_into(JsonDocument &doc) noexcept {
    const auto err = deserializeMsgPack(doc, raw_in(), xfr.in);
    consume(xfr.in);

    if (err) {
      ESP_LOGW(TAG, "deserialize err=%s\n", err.c_str());
    }

    return !err;
  }

  /// @brief Returns the DMX frame data from the JSON document
  /// @tparam T type expected in the data arrauy
  /// @return Populated T transferred from JSON array
  template <typename T> static inline T dframe(const JsonDocument &doc) {
    if (auto array = doc[DFRAME].as<JsonArrayConst>(); array) {
      return T(array);
    }

    ESP_LOGI(TAG, "dframe(): returning default T");

    return T();
  }

  inline void reuse() noexcept {
    packed_len = 0;
    ec = error_code();
    xfr.bytes = 0;
    e.reset();
  }

  static const string type(const JsonDocument &doc) noexcept {
    const char *type_cstr = doc[MSG_TYPE];

    return type_cstr ? string(type_cstr) : string(desk::UNKNOWN);
  }

public:
  static constexpr auto TAG{"desk.msg.in"};
};

} // namespace desk
} // namespace ruth