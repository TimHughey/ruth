
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
#include <concepts>
#include <esp_log.h>

namespace ruth {
namespace desk {

class MsgIn : public Msg<packed_in_t> {

public:
  inline MsgIn(packed_in_t &packed) noexcept : Msg(packed) {}

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
  inline bool calc_packed_len() noexcept {

    // get packed_len if we don't already have it and there are
    // header bytes available
    if (!packed_len && (xfr.in >= hdr_bytes)) {
      char *packed_len_bytes = (char *)(&packed_len);

      packed_len_bytes[0] = packed[0];
      packed_len_bytes[1] = packed[1];

      packed_len = ntohs(packed_len);
    }

    return packed_len;
  }

  /// @brief Determines if the message can be rendered
  /// @return boolean
  template <typename Doc> bool can_render(const Doc &doc) const noexcept {
    return xfer_ok() && doc[desk::MAGIC] == MAGIC_VAL;
  }

  /// @brief Deserializes the packed message into the provided JsonDocument
  /// @tparam Doc Any JsonDocument type (Static or Dynamic)
  /// @param doc Reference to the JsonDocument to populate
  /// @return boolean indicating if deserialization successful
  template <typename Doc> inline auto deserialize_into(Doc &doc) noexcept {
    // size we allocated the exact size of the expected packed data then
    // returned a buffer to the container we can use it without fussing
    // with the header

    // the error returned is zero (0) so we must negate it
    // to return true for success
    return !deserializeMsgPack(doc, packed.data(), packed.size());
  }

  /// @brief Returns the DMX frame data from the JSON document
  /// @tparam T type expected in the data arrauy
  /// @return Populated T transferred from JSON array
  template <typename T> inline T dframe(const JsonDocument &doc) const {
    if (auto array = doc[DFRAME].as<JsonArrayConst>(); array) {
      return T(array);
    }

    ESP_LOGI(TAG, "dframe(): returning default T");

    return T();
  }

  /// @brief  create buffer sequence for initial message read
  ///         of up to 25% of max size
  /// @return std::array of asio::mutable buffers
  auto read_initial_buffs() noexcept {
    // ensure buffer for initial read of header (message length)
    packed.assign(hdr_bytes, 0x00);

    // packed was initialized with enough bytes for the header
    // asio::buffer() will create a buffer of the size of the container
    return asio::buffer(packed);
  }

  auto read_intermediate_buff() noexcept {
    // for the content read we only want the packed_len. so assign packed_len
    // to the container and return an asio::buffer
    packed.assign(packed_len, 0x00);

    return asio::buffer(packed);
  }

public:
  static constexpr const auto TAG{"desk::msg_in"};
};

} // namespace desk
} // namespace ruth