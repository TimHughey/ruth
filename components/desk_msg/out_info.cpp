//  Ruth
//  Copyright (C) 2023  Tim Hughey
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

#include "desk_msg/out_info.hpp"

#include <esp_system.h>

namespace ruth {
namespace desk {

void MsgOutWithInfo::add_heap_info(JsonDocument &doc) noexcept {

  JsonObject heap = doc.createNestedObject("heap");
  heap["min"] = esp_get_minimum_free_heap_size();
  heap["free"] = esp_get_free_heap_size();
  heap["max_alloc"] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

} // namespace desk
} // namespace ruth