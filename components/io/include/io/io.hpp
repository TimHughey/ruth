
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

#include <asio.hpp>
#include <cstdint>

namespace ruth {

using error_code = asio::error_code;
using io_context = asio::io_context;
using ip_address = asio::ip::address;
using ip_tcp = asio::ip::tcp;
using ip_udp = asio::ip::udp;
using steady_timer = asio::steady_timer;
using strand = io_context::strand;
using tcp_acceptor = asio::ip::tcp::acceptor;
using tcp_endpoint = asio::ip::tcp::endpoint;
using tcp_socket = asio::ip::tcp::socket;
using udp_endpoint = asio::ip::udp::endpoint;
using udp_socket = asio::ip::udp::socket;

typedef uint16_t Port;

} // namespace ruth