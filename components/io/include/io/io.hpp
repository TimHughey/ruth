
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

#include <array>
#include <asio.hpp>
#include <cstdint>
#include <string_view>
#include <system_error>

namespace ruth {

using const_buff = asio::const_buffer;
using errc = std::errc;
using error_code = std::error_code;
using io_context = asio::io_context;
using ip_address = asio::ip::address;
using ip_tcp = asio::ip::tcp;
using ip_udp = asio::ip::udp;
using mut_buffer = asio::mutable_buffer;
using socket_base = asio::socket_base;
using steady_timer = asio::steady_timer;
using system_timer = asio::system_timer;
using strand = io_context::strand;
using tcp_acceptor = asio::ip::tcp::acceptor;
using tcp_endpoint = asio::ip::tcp::endpoint;
using tcp_socket = asio::ip::tcp::socket;
using udp_endpoint = asio::ip::udp::endpoint;
using udp_socket = asio::ip::udp::socket;
using work_guard = asio::executor_work_guard<asio::io_context::executor_type>;

typedef uint16_t Port;
namespace io {
constexpr auto noent = std::errc::no_such_file_or_directory;

void log_accept_socket(const std::string_view module_id, const std::string_view type,
                       tcp_socket &sock, const tcp_endpoint &r, bool log = true) noexcept;

inline auto make_error() noexcept { return std::error_code(); }
inline auto make_error(errc val) noexcept { return std::make_error_code(val); }

typedef std::array<char, 1024> Packed;

} // namespace io

} // namespace ruth