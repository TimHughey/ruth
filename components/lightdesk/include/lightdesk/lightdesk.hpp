
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
#include "ru_base/rut.hpp"

#include <chrono>
#include <esp_timer.h>
#include <memory>
#include <optional>

namespace ruth {
class LightDesk;

namespace desk {
class Session;
}

namespace shared {
extern std::optional<LightDesk> desk;
extern std::optional<desk::Session> session;
} // namespace shared

class LightDesk {
public:
  LightDesk() noexcept;
  ~LightDesk() = default;

  void run() noexcept;

private:
  void async_accept() noexcept;

private:
  // order dependent
  io_context io_ctx;
  tcp_acceptor acceptor;

  // order independent
  std::optional<tcp_socket> peer;

public:
  static constexpr csv SERVICE_NAME{"_ruth"};
  static constexpr csv SERVICE_PROTOCOL{"_tcp"};
  static constexpr Port SERVICE_PORT{49152};
  static constexpr csv TAG{"lightdesk"};
};

} // namespace ruth
