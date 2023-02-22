
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

#include "async/msg_keys.hpp"
#include "io/io.hpp"
#include "misc/elapsed.hpp"
#include "ru_base/time.hpp"
#include "ru_base/types.hpp"

#include <ArduinoJson.h>
#include <arpa/inet.h>
#include <array>
#include <esp_log.h>
#include <functional>
#include <istream>
#include <memory>
#include <optional>
#include <vector>

namespace ruth {
namespace desk {

using packed_t = asio::streambuf;
using StaticDoc = StaticJsonDocument<640>;

class MsgIn {
public:
  static constexpr std::size_t default_packed_size{1024};
  static constexpr std::size_t min_buff_size{default_packed_size / 4};

public:
  inline MsgIn(packed_t &packed) noexcept
      : packed(packed),               //
        packed_len{0},                //
        hdr_bytes{sizeof(packed_len)} //
  {}

  inline ~MsgIn() noexcept {}        // prevent default copy/move
  inline MsgIn(MsgIn &&m) = default; // allow move
  inline MsgIn &operator=(MsgIn &&m) = default;

  /// @brief   Calculate the packed length of the associaeted stream buffer.
  ///          This function can be called as many times as needed until
  ///          the stream buffer contains a complete packed message.
  ///          note:  bytes are only committed to the stream buffer
  /// @param n optional number of bytes read into buffer.  when called
  ///          without n (or n==0) examines the stream buffer for a
  ///          for a complete message
  /// @return  true the stream buffer contains a complete message
  ///          false otherwise
  inline bool calc_packed_len(std::size_t n = 0) noexcept {

    packed.commit(n); // commit bytes, if any, written to a prepared buffer

    // get packed_len if we don't already have it and there are
    // header bytes available
    if (!packed_len && (packed.size() >= hdr_bytes)) {
      const void *plrv = packed.data().data();
      const uint16_t *plr = reinterpret_cast<const uint16_t *>(plrv);

      packed_len = ntohs(*plr);
    }

    // calculate if we need additional bytes to complete
    // the entire packed message
    if ((packed.size() - hdr_bytes) >= packed_len) {
      need_bytes = 0;
    } else {
      need_bytes = packed_len - packed.size() - hdr_bytes;
    }

    return packed_len && !need_bytes;
  }

  /// @brief  "Smartly" adds key/value pair to JSON document
  /// @tparam T value type
  /// @param key the key
  /// @param val the val, supports Elapsed, chrono objects and integrals
  template <typename T> inline void add_kv(csv key, T val) {
    auto &doc = doc_ref.value().get();

    if constexpr (std::is_same_v<T, Elapsed>) {
      doc[key] = val().count();
    } else if constexpr (std::is_same_v<T, Nanos> || std::is_same_v<T, Micros> ||
                         std::is_same_v<T, Millis>) {
      doc[key] = val.count();
    } else {
      doc[key] = val;
    }
  }

  /// @brief Determines if the message can be rendered
  /// @return boolean
  inline bool can_render() const noexcept {
    return !need_bytes && doc_ref.value().get()[desk::MAGIC] == MAGIC_VAL;
  }

  /// @brief Deserializes the packed message into the provided JsonDocument
  /// @tparam Doc Any JsonDocument type (Static or Dynamic
  /// @param doc Reference to the JsonDocument to populate
  /// @return boolean indicating if deserialization successful
  template <typename Doc> inline auto deserialize_into(Doc &doc) noexcept {
    doc_ref.emplace(doc);

    packed.consume(hdr_bytes);
    const char *data = static_cast<const char *>(packed.data().data());

    const auto err = deserializeMsgPack(doc, data, packed_len);
    packed.consume(packed_len);

    return !err;
  }

  /// @brief Returns the DMX frame data from the JSON document
  /// @tparam T type expected in the data arrauy
  /// @return Populated T transferred from JSON array
  template <typename T> inline T dframe() const {
    if (auto array = doc_ref.value().get()[DFRAME].as<JsonArrayConst>(); array) {
      return T(array);
    }

    ESP_LOGI(TAG, "dframe(): returning default T");

    return T();
  }

  /// @brief  create buffer sequence for initial message read
  ///         of up to 25% of max size
  /// @return std::array of asio::mutable buffers
  auto read_initial_buffs() noexcept { return packed.prepare(min_buff_size); }

  auto read_intermediate_buff() noexcept {
    ESP_LOGI(TAG, "max=%u cap=%u size=%u packed_len=%u need=%ld", packed.max_size(),
             packed.capacity(), packed.size(), packed_len, need_bytes);

    return packed.prepare(256);
  }

  /// @brief return token indicating bytes to transfer on first read
  /// @return asio ReadToken
  inline auto read_initial_bytes() noexcept { return asio::transfer_at_least(hdr_bytes); }

  /// @brief return token indicating bytes to transfer on subsequent reads
  /// @return asio ReadToken
  inline auto read_rest_bytes() noexcept {
    return asio::transfer_at_least(std::exchange(need_bytes, 0));
  }

  /// @brief Reset the elapsed timer
  inline void restart_elapsed() noexcept { e.reset(); }

  // order dependent
  packed_t &packed; // buffer for io, owned by caller
  volatile std::uint16_t packed_len{0};
  const std::size_t hdr_bytes;

  // order independent
  int32_t need_bytes{0};
  std::optional<std::reference_wrapper<JsonDocument>> doc_ref;
  Elapsed e;

public:
  static constexpr const auto TAG{"desk::msg_in"};
};

} // namespace desk
} // namespace ruth