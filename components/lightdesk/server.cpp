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

#include "server/server.hpp"
#include "ru_base/types.hpp"
#include "session/session.hpp"

#include <esp_log.h>

namespace ruth {
namespace desk {

Server::~Server() {
  [[maybe_unused]] auto handle = acceptor.native_handle();
  [[maybe_unused]] error_code ec;
  acceptor.close(ec); // use error_code overload to prevent throws
}

void Server::asyncLoop(const error_code ec_last) {
  // first things first, check ec_last passed in, bail out if needed
  if (ec_last || !acceptor.is_open()) { // problem
    // don't highlight "normal" shutdown
    if ((ec_last != io::aborted) && (ec_last != io::resource_unavailable)) {
      ESP_LOGW(serverID(), "accept failed, reason=%s", ec_last.message().c_str());
    }
    // some kind of error occurred, simply close the socket
    [[maybe_unused]] error_code ec;
    acceptor.close(ec); // used error code overload to prevent throws

    return; // bail out
  }

  // this is the socket for the next accepted connection, store it in an
  // optional for the lamba
  socket.emplace(di.io_ctx);

  // since the io_ctx is wrapped in the optional and asyncLoop wants the actual
  // io_ctx we must deference or get the value of the optional
  acceptor.async_accept(*socket, [&](const error_code ec) {
    if (!ec) {

      // allow only one active session
      if (!Session::activeSession()) {
        ESP_LOGI(serverID(), "accepted connection, handle=%0x", socket->native_handle());

        // create the session passing all the options
        // notes
        //  1: we move the socket (value of the optional) to session
        //  2. we then start the session using the shared_ptr returned by Session::create()
        //  3. Session::start() must ensure the shared_ptr pointer is captured in the
        //     async lamba so it doesn't go out of scope

        // assemble the dependency injection and start the server
        const session::Inject inject{.io_ctx = di.io_ctx, // io_cty (used to create a local strand)
                                     .socket = std::move(socket.value()),
                                     .idle_shutdown = di.idle_shutdown,
                                     .idle_check = di.idle_check};

        Session::start(inject);
      } else { // already have an active session
        [[maybe_unused]] error_code ec;
        socket->shutdown(tcp_socket::shutdown_both);
        socket->close();
      }

      asyncLoop(ec); // schedule more work or gracefully exit
    }
  });
}

void Server::teardown() {
  // here we only issue the cancel to the acceptor.
  // the closing of the acceptor will be handled when
  // the error is caught by asyncLoop

  [[maybe_unused]] error_code ec;
  acceptor.cancel(ec);
}

} // namespace desk
} // namespace ruth
