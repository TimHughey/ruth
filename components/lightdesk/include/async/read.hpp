
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

#include <array>
#include <esp_log.h>
#include <functional>
#include <istream>
#include <memory>
#include <type_traits>
#include <utility>

namespace ruth {
namespace desk {

/// @brief initiate an async read of a lightdesk message
/// @tparam T
/// @tparam CompletionToken
/// @param sock
/// @param msg
/// @param token
/// @return
template <typename T, typename CompletionToken>
inline auto async_read(tcp_socket &sock, T &&msg, CompletionToken &&token) {

  auto initiation = [](auto &&comp_handler, tcp_socket &sock, T &&msg) {
    using handler_t = std::decay<decltype(comp_handler)>::type;

    struct intermediate_token {
      tcp_socket &sock; // for the second async_read() and obtaining the I/O executor
      T msg;
      enum state_t { INITIAL = 100, MORE = 200 } state;
      handler_t handler;

      inline void operator()(const error_code &ec, std::size_t n = 0) {
        // static const char *TAG{"desk::async_read"};

        // ESP_LOGI(TAG, "ec=%s n=%u", ec.message().c_str(), n);

        if (!ec) { // ensure no socket errors

          // on the initial transfer reset the elapsed time
          // yes, this loses the actual time from when the hardware
          // received the data and the passing of the data through
          // the OS.  we reset so we don't include the wait time
          // between packets.  this elapsed time is purely to measure
          // the internal ruth processing time (including any
          // additional network reads)
          if (state == state_t::INITIAL) msg.restart_elapsed();

          if (msg.calc_packed_len(n) == false) { // packed is incomplete
            state = state_t::MORE;
            asio::async_read(sock,                         //
                             msg.read_intermediate_buff(), //
                             msg.read_rest_bytes(),        //
                             std::move(*this));
            return;
          }

          // we have the entire msg on the first read, fall through to handler
        }

        // we now have either an error or a complete message, call the handler
        handler(ec, std::move(msg));
      };

      using executor_type =
          asio::associated_executor_t<typename std::decay<decltype(comp_handler)>::type,
                                      tcp_socket::executor_type>;

      inline executor_type get_executor() const noexcept {
        return asio::get_associated_executor(handler, sock.get_executor());
      }

      using allocator_type =
          asio::associated_allocator_t<typename std::decay<decltype(comp_handler)>::type,
                                       std::allocator<void>>;

      inline allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(handler, std::allocator<void>{});
      }
    }; // end intermediate struct

    // Initiate the underlying async_read operation using our intermediate
    // completion handler.

    asio::async_read(sock,                     //
                     msg.read_initial_buffs(), //
                     msg.read_initial_bytes(),
                     intermediate_token{
                         sock,
                         std::forward<T>(msg),                              // the msg
                         intermediate_token::state_t::INITIAL,              //
                         std::forward<decltype(comp_handler)>(comp_handler) // handler
                     });
  };

  return asio::async_initiate<CompletionToken, void(error_code, T && msg)>(
      initiation,          // initiation function object
      token,               // user supplied callback
      std::ref(sock),      // wrap non-const args to prevent incorrect decay-copies
      std::forward<T>(msg) // create for use within the async operation
  );
}

} // namespace desk
} // namespace ruth