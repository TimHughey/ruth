
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

#include "misc/elapsed.hpp"
#include "msg/kv.hpp"
#include "ru_base/types.hpp"

#include <ArduinoJson.h>
#include <asio/error.hpp>
#include <asio/streambuf.hpp>
#include <memory>

namespace ruth {
namespace desk {

class Msg {

public:
  inline Msg(std::size_t capacity) noexcept
      : storage(std::make_unique<asio::streambuf>(capacity)) {}
  virtual ~Msg() noexcept {} // prevent implicit copy/move

  inline Msg(Msg &&m) = default;       // allow move construct
  Msg &operator=(Msg &&msg) = default; // allow move assignment

  inline auto &buffer() noexcept { return *storage; }
  inline void consume(std::size_t n) noexcept { storage->consume(n); }

  inline auto elapsed() noexcept { return e.freeze(); }
  inline auto elapsed_restart() noexcept { return e.reset(); }

  inline auto in_avail() noexcept { return storage->in_avail(); }

  inline bool is_msg_type(const JsonDocument &doc, csv want_type) const noexcept {
    return want_type == csv{doc[MSG_TYPE].as<const char *>()};
  }

  inline bool xfer_error() const noexcept { return !xfer_ok(); }
  inline bool xfer_ok() const noexcept { return !ec && (xfr.bytes >= packed_len); }

protected:
protected:
  // order dependent
  std::unique_ptr<asio::streambuf> storage;

public:
  // order independent
  uint16_t packed_len{0};
  asio::error_code ec;

  union {
    size_t in;
    size_t out;
    size_t bytes;
  } xfr{0};

protected:
  Elapsed e; // duration tracking

public:
  static constexpr size_t default_doc_size{6 * 128};
  static constexpr csv module_id{"desk.msg"};
};

} // namespace desk
} // namespace ruth