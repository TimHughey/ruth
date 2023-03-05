
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
#include "ru_base/types.hpp"

#include <ArduinoJson.h>
#include <esp_log.h>
#include <functional>
#include <istream>
#include <memory>
#include <type_traits>
#include <utility>

namespace ruth {
namespace desk {
namespace async {

/// @brief Async write desk msg
/// @tparam T
/// @tparam CompletionToken
/// @param socket
/// @param msg
/// @param token
/// @return
template <typename T, typename CompletionToken>
auto write_msg2(tcp_socket &socket, T &&msg, JsonDocument &doc, CompletionToken &&token) {

  auto initiation = [](auto &&completion_handler, tcp_socket &socket, T &&msg) {
    struct intermediate_completion_handler {
      tcp_socket &socket; // for multiple write ops and obtaining I/O executor
      T msg;

      typename std::decay<decltype(completion_handler)>::type handler;

      void operator()(const error_code &ec, std::size_t n) noexcept {
        msg.ec = ec;
        msg.xfr.out = n;

        handler(std::move(msg)); // call user-supplied handler
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
    auto &write_buff = msg.write_buff();

    // initiate the actual async operation
    asio::async_write(socket, write_buff,
                      intermediate_completion_handler{
                          socket, std::forward<T>(msg),
                          std::forward<decltype(completion_handler)>(completion_handler)});
  };

  msg.serialize(doc);

  // initiate the async operation
  return asio::async_initiate<CompletionToken, void(T msg)>(
      initiation,          // initiation function object
      token,               // user supplied callback
      std::ref(socket),    // wrap non-const args to prevent incorrect decay-copies
      std::forward<T>(msg) // move the msg for use within the async operation
  );
}

} // namespace async
} // namespace desk
} // namespace ruth
