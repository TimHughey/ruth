
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

#include "desk_msg/msg.hpp"
#include "io/io.hpp"

#include <ArduinoJson.h>
#include <esp_log.h>
#include <memory>

namespace ruth {
namespace desk {

class MsgIn : public Msg {

public:
  inline MsgIn() : Msg(512) {}
  ~MsgIn() noexcept {}

  inline MsgIn(MsgIn &&) = default;
  MsgIn &operator=(MsgIn &&) = default;

  inline void operator()(const error_code &op_ec, size_t n) noexcept {
    xfr.in += n;
    ec = op_ec;
    packed_len = n; // should we need to set this?

    if (n == 0) ESP_LOGD(TAG, "SHORT READ  n=%d err=%s\n", xfr.in, ec.message().c_str());
  }

  inline auto deserialize_into(JsonDocument &doc) noexcept {
    const auto err = deserializeMsgPack(doc, raw(), xfr.in);
    consume(xfr.in);

    // ESP_LOGI(TAG, "xfr.in=%u mem_used=%u", xfr.in, doc.memoryUsage());

    if (err) {
      ESP_LOGW(TAG, "deserialize err=%s", err.c_str());
    }

    return !err;
  }

public:
  static auto constexpr TAG{"desk.msg.in"};
};

} // namespace desk
} // namespace ruth