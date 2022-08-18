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

typedef std::array<char, 384> Packed;

class DeskMsg {

private:
  static constexpr csv TAG{"DeskMsg"};

  // doc keys
  static constexpr csv MAGIC{"magic"};
  static constexpr csv SEQ_NUM{"seq_num"};
  static constexpr csv DFRAME{"dframe"};

  // doc constant values
  static constexpr uint32_t MAGIC_VAL{0xc9d2};

  static constexpr size_t DOC_SIZE = 2048; // JSON_ARRAY_SIZE(64) + JSON_OBJECT_SIZE(13);

public:
  inline DeskMsg(Packed &buff, size_t rx_bytes, int64_t async_us) : async_us(async_us) {
    if (auto err = deserializeMsgPack(doc, buff.data(), rx_bytes); err) {
      ESP_LOGW(TAG.data(), "deserialize failure reason=%s", err.c_str());
      deserialize_ok = false;
    } else {
      deserialize_ok = true;
      root_obj = doc.as<JsonObjectConst>();

      // uint32_t timestamp = root_obj["timestamp"].as<uint32_t>();
      // int64_t remote_now = root_obj["now_µs"].as<int64_t>();

      // bool silence = root_obj["silence"];

      // if (auto v = ru_time::now_epoch<Micros>().count() - remote_now; v > 10000) {
      //   int64_t nettime_now = root_obj["nettime_now_µs"].as<int64_t>();
      //   int64_t frame_local = root_obj["frame_localtime_µs"].as<int64_t>();
      //   int64_t diff = std::abs(nettime_now - frame_local);
      //   ESP_LOGI(TAG.data(), "variance=%lld src diff=%lld", v, diff);
      // }
    }
  }

  inline auto root() const { return root_obj; }
  inline bool good() const { return deserialize_ok; }

public:
  template <typename T> inline T dframe() const {
    T dmx_f = T();

    if (auto dframe_array = root_obj[DFRAME].as<JsonArrayConst>(); dframe_array) {
      size_t idx = 0;
      for (JsonVariantConst frame_byte : root_obj[DFRAME].as<JsonArrayConst>()) {
        dmx_f[idx++] = frame_byte.as<uint8_t>();
      }
    }

    return std::move(dmx_f);
  }

  inline bool playable() const {
    uint32_t seq_num = root_obj[SEQ_NUM];
    uint32_t magic = root_obj[MAGIC];

    bool can_play = deserialize_ok && (magic == MAGIC_VAL);

    if (!can_play) {
      ESP_LOGW(TAG.data(), "not playable deserialize=%s magic=%u",
               deserialize_ok ? "ok" : "failed", magic);
    }

    ESP_LOGD(TAG.data(), "seq=num=%u async_us=%lld", seq_num, async_us);

    return can_play;
  }

  inline size_t inspect(string &json_debug) const { return serializeJsonPretty(doc, json_debug); }

private:
  // order dependent
  int64_t async_us;

  // order independent
  StaticJsonDocument<DOC_SIZE> doc;
  JsonObjectConst root_obj;

  bool deserialize_ok = false;
};

} // namespace ruth
