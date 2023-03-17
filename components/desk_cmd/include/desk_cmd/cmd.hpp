
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
#include "desk_msg/out.hpp"

#include <ArduinoJson.h>
#include <esp_log.h>
#include <io/io.hpp>
#include <memory>

namespace ruth {
namespace desk {

class Cmd : public Msg, public std::enable_shared_from_this<Cmd> {

public:
  Cmd(tcp_socket &&sock) : Msg(512), sock(std::move(sock)) {}

  ~Cmd() noexcept override {
    [[maybe_unused]] asio::error_code ec;
    sock.shutdown(tcp_socket::shutdown_both, ec);
    sock.close(ec);
  }

  inline Cmd(Cmd &&) = default;
  Cmd &operator=(Cmd &&) = default;

  // keep this inlined for asio async
  inline void operator()(const error_code &op_ec, size_t n) noexcept {
    xfr.in += n;
    ec = op_ec;
    packed_len = n; // should we need to set this?

    if (n == 0) ESP_LOGD(TAG, "SHORT READ  n=%d err=%s\n", xfr.in, ec.message().c_str());
  }

  static std::shared_ptr<Cmd> create(tcp_socket &&sock) noexcept {
    auto cmd = std::make_shared<Cmd>(std::forward<tcp_socket>(sock));
    return cmd->shared_from_this();
  }

  // ok for this to be in flash, not concerned about speed of execution
  bool deserialize_into(JsonDocument &doc) noexcept;

  bool process() noexcept;

  tcp_socket &socket() noexcept { return sock; }

private:
  void send_response(MsgOut &&m) noexcept;

private:
  tcp_socket sock;

public:
  static constexpr auto TAG{"desk.msg.cmd"};
};

} // namespace desk
} // namespace ruth