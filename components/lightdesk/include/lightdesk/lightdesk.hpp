
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

#include "ArduinoJson.h"
#include "io/io.hpp"
#include "ru_base/rut.hpp"

#include <chrono>
#include <memory>

namespace ruth {
class Binder;

// forward decl to avoid pulling in session.hpp
namespace desk {
class Session;
}

class LightDesk {
public:
  LightDesk() noexcept;
  ~LightDesk() noexcept {} // prevent implict copy/move

  void advertise(Binder *binder) noexcept; // in .cpp to hide mdns
  void async_accept_cmd() noexcept;        // in .cpp to hide command processing
  void async_accept_data() noexcept;       // in .cpp to hide Session impl

  void run(Binder *binder) noexcept; // in .cpp to hide Binder, Net

public:
  // order dependent
  io_context io_ctx;
  io_context io_ctx_session;
  tcp_acceptor acceptor_data;
  tcp_acceptor acceptor_cmd;

public:
  static constexpr Port CMD_PORT{49151};
  static constexpr csv SERVICE_NAME{"_ruth"};
  static constexpr csv SERVICE_PROTOCOL{"_tcp"};
  static constexpr Port SERVICE_PORT{49152};
  static constexpr const auto TAG{"lightdesk"};
};

} // namespace ruth

//
// free functions
//

void ruth_main() noexcept;
