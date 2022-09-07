
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
#include "io/msg_static.hpp"
#include "ru_base/types.hpp"

#include <array>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace ruth {
namespace io {

//
// async_read_msg(): reads a message, prefixed by length
//

template <typename B, typename CompletionToken>
inline auto async_read_msg(tcp_socket &socket, B &static_buff, CompletionToken &&token) {
  auto initiation = [](auto &&completion_handler, tcp_socket &socket, Msg msg) {
    struct intermediate_completion_handler {
      tcp_socket &socket; // for the second async_read() and obtaining the I/O executor
      Msg msg;            // hold the in-flight M
      typename std::decay<decltype(completion_handler)>::type handler;

      inline void operator()(const error_code &ec, std::size_t n = 0) {
        error_code ec_local = ec;

        if (!ec_local && (n == MSG_LEN_SIZE)) {

          auto bytes = socket.read_some(msg.buff_packed(), ec_local);

          if (msg.deserialize(ec_local, bytes) == false) {
            ec_local = make_error(errc::message_size);
          }
        }

        // This point is reached only on completion of the entire composed
        // operation.

        // Call the user-supplied handler with the result of the operation.
        handler(std::move(ec_local), std::move(msg));
      };

      using executor_type =
          asio::associated_executor_t<typename std::decay<decltype(completion_handler)>::type,
                                      tcp_socket::executor_type>;

      inline executor_type get_executor() const noexcept {
        return asio::get_associated_executor(handler, socket.get_executor());
      }

      using allocator_type =
          asio::associated_allocator_t<typename std::decay<decltype(completion_handler)>::type,
                                       std::allocator<void>>;

      inline allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(handler, std::allocator<void>{});
      }
    }; // end intermediate struct

    // Initiate the underlying async_read operation using our intermediate
    // completion handler.

    auto msg_len = msg.buff_msg_len();
    asio::async_read(socket, msg_len, asio::transfer_exactly(MSG_LEN_SIZE),
                     intermediate_completion_handler{
                         socket,         // the socket
                         std::move(msg), // the msg
                         std::forward<decltype(completion_handler)>(completion_handler) // handler
                     });
  };

  return asio::async_initiate<CompletionToken, void(error_code, Msg msg)>(
      initiation,       // initiation function object
      token,            // user supplied callback
      std::ref(socket), // wrap non-const args to prevent incorrect decay-copies
      Msg(static_buff)  // create for use within the async operation
  );
}

inline auto write_msg(tcp_socket &socket, Msg &msg) {
  error_code ec;

  msg.serialize();

  // must grab the buff_seq and tx_len BEFORE moving the msg
  auto bytes = socket.write_some(msg.buff_tx(), ec);

  if (!ec && (bytes == msg.tx_len)) {
    return ec;
  }

  return io::make_error(errc::message_size);
}

//
// async_write_msg(): write a message object to the socket
//
/*
template <typename CompletionToken>
inline auto async_write_msg(tcp_socket &socket, std::unique_ptr<Msg> msg,
                            CompletionToken &&token) {

  auto initiation = [](auto &&completion_handler, tcp_socket &socket, std::unique_ptr<Msg> msg)
{ struct intermediate_completion_handler { tcp_socket &socket; // for multiple write ops and
obtaining I/O executor std::unique_ptr<Msg> msg;

      typename std::decay<decltype(completion_handler)>::type handler;

      void operator()(const error_code &ec, std::size_t bytes) {
        msg->log_tx(ec, bytes);

        handler(ec); // call user-supplied handler
      }

      using executor_type =
          asio::associated_executor_t<typename std::decay<decltype(completion_handler)>::type,
                                      tcp_socket::executor_type>;

      executor_type get_executor() const noexcept {
        return asio::get_associated_executor(handler, socket.get_executor());
      }

      using allocator_type =
          asio::associated_allocator_t<typename std::decay<decltype(completion_handler)>::type,
                                       std::allocator<void>>;

      allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(handler, std::allocator<void>{});
      }
    };

    // must grab the buff_seq and tx_len BEFORE moving the msg
    auto buff_tx = msg->buff_tx();
    auto tx_len = msg->tx_len;

    // initiate the actual async operation
    asio::async_write(
        socket, buff_tx, asio::transfer_exactly(tx_len),
        intermediate_completion_handler{
            socket,                                                        // pass the socket
along std::move(msg),                                                // move the msg along
            std::forward<decltype(completion_handler)>(completion_handler) // forward token
        });
  };

  msg->serialize();

  // initiate the async operation
  return asio::async_initiate<CompletionToken, void(error_code)>(
      initiation,       // initiation function object
      token,            // user supplied callback
      std::ref(socket), // wrap non-const args to prevent incorrect decay-copies
      std::move(msg)    // move the msg for use within the async operation
  );
}
*/
} // namespace io
} // namespace ruth