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

#include "inject/inject.hpp"
#include "io/io.hpp"

#include <list>
#include <optional>

namespace ruth {
namespace desk {

class Server {
public:
  Server(const server::Inject &inject) noexcept
      : di(inject), // safe to save injected deps here
        acceptor{di.io_ctx, tcp_endpoint{ip_tcp::v4(), di.listen_port}} // use the injected io_ctx
  {}

  ~Server() noexcept;

  // asyncLoop is invoked to:
  //  1. schedule the initial async accept
  //  2. after accepting a connection to schedule the next async connect
  //
  // with this in mind we accept an error code that is checked before
  // starting the next async_accept.  when the error code is not specified
  // assume this is startup and all is well.
  void asyncLoop(const error_code ec_last = error_code()) noexcept;

  Port localPort() noexcept { return acceptor.local_endpoint().port(); }
  void teardown() noexcept; // issue cancel to acceptor
  void shutdown() noexcept { teardown(); }

private:
  // order dependent
  const server::Inject di; // make a copy
  tcp_acceptor acceptor;

  std::optional<tcp_socket> socket;

public:
  static constexpr csv server_id{"desk"};
};

} // namespace desk
} // namespace ruth
