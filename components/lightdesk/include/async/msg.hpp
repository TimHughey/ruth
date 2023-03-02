
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

#include "io/io.hpp"
#include "misc/elapsed.hpp"
#include "ru_base/types.hpp"

#include <ArduinoJson.h>
#include <concepts>
#include <esp_log.h>
#include <vector>

namespace ruth {
namespace desk {

using packed_in_t = std::vector<char>;
using packed_out_t = std::vector<char>;
using StaticDoc = StaticJsonDocument<640>;

template <class PACKED> class Msg {
public:
  static constexpr std::size_t default_packed_len{1024};

public:
  inline Msg(PACKED &packed) noexcept
      : packed(packed),               //
        packed_len{0},                //
        hdr_bytes{sizeof(packed_len)} //
  {}

  virtual inline ~Msg() noexcept {}         // prevent default copy/move
  inline Msg(Msg &&m) = default;            // allow move construct
  inline Msg &operator=(Msg &&m) = default; // allow move assignment

  inline void copy_kv(JsonDocument &doc_a, JsonDocument &doc_b, csv key) noexcept {
    doc_b[key] = doc_a[key];
  }

  inline auto elapsed() noexcept { return e.freeze(); }
  inline auto elapsed_restart() noexcept { return e.reset(); }

  inline bool xfer_error() const noexcept { return !xfer_ok(); }
  inline bool xfer_ok() const noexcept {

    auto rc = !ec && (xfr.bytes >= packed_len);

    if (!rc) {
      ESP_LOGI(TAG, "xfr.bytes=%u packed_len=%u ec=%s", xfr.bytes, packed_len,
               ec.message().c_str());
    }

    return rc;
  }

public:
  // order dependent
  PACKED &packed; // buffer for io, owned by caller
  volatile std::uint16_t packed_len{0};
  const std::size_t hdr_bytes;

  // order independent
  error_code ec;
  union {
    std::size_t in;
    std::size_t out;
    std::size_t bytes;
  } xfr{0};

private:
  Elapsed e;

public:
  static constexpr auto TAG{"desk::msg"};
};

} // namespace desk
} // namespace ruth