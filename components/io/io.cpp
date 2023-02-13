
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

#include "io/io.hpp"

#include <esp_log.h>
#include <esp_types.h>

namespace ruth {
namespace io {

void log_accept_socket(const std::string_view module_id, const std::string_view type, tcp_socket &s,
                       const tcp_endpoint &r, bool log) noexcept {

  if (log) {
    const auto &l = s.local_endpoint();

    ESP_LOGI(module_id.data(), "%s local=%s:%d remote=%s:%d connected, handle=0x%0x",
             type.data(),                               //
             l.address().to_string().c_str(), l.port(), //
             r.address().to_string().c_str(), r.port(), //
             s.native_handle());
  }
}

error_code make_error(errc e) { return asio::error::make_error_code(e); }
} // namespace io

} // namespace ruth