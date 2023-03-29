
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

#include "desk_cmd/cmd.hpp"
#include "async_msg/write.hpp"
#include "desk_cmd/ota.hpp"
#include "desk_msg/kv.hpp"
#include "ru_base/clock_now.hpp"
#include <desk_msg/out.hpp>

#include <ArduinoJson.h>
#include <esp_attr.h>
#include <esp_log.h>

namespace ruth {
namespace desk {

bool Cmd::deserialize_into(JsonDocument &doc) noexcept {
  const auto err = deserializeMsgPack(doc, raw(), xfr.in);
  consume(xfr.in);

  if (err) ESP_LOGW(TAG, "deserialize err=%s", err.c_str());

  return !err;
}

bool Cmd::process() noexcept {

  DynamicJsonDocument doc_in(Msg::default_doc_size);

  const auto in_len{xfr.in};

  if (deserialize_into(doc_in)) {

    ESP_LOGI(TAG, "in_len=%u memory_usage: %u", in_len, doc_in.memoryUsage());

    if (is_msg_type(doc_in, desk::PING)) {

      int64_t local_us = clock_now::real::us();
      int64_t remote_us = doc_in[desk::NOW_REAL_US].as<int64_t>();
      const auto diff_us = local_us - remote_us;

      MsgOut msg_out(desk::PONG);

      msg_out.add_kv(desk::TEXT, "pong");
      msg_out.add_kv(desk::ELAPSED_US, elapsed());
      msg_out.add_kv(desk::DIFF_REAL_US, diff_us);
      send_response(std::move(msg_out));

    } else if (is_msg_type(doc_in, desk::OTA_REQUEST)) {
      const char *url = doc_in[desk::URL].as<const char *>();
      const char *file = doc_in[desk::FILE].as<const char *>();

      OTA::create(std::move(sock), url, file)->execute();
    }
  }
  return true;
}

// NOTE:  async_msg::write() is inlined.  we create a wrapper here
//        to avoid code bloat for desk cmds
void IRAM_ATTR Cmd::send_response(MsgOut &&msg) noexcept {

  async_msg::write(sock, std::move(msg), [self = shared_from_this()](MsgOut msg_out) {
    if (msg_out.xfer_error()) {
      ESP_LOGI(TAG, "write reply error %s", msg_out.ec.message().c_str());
    }
  });
}

} // namespace desk
} // namespace ruth
