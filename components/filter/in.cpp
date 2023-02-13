/*
  Ruth
  (C)opyright 2021  Tim Hughey

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  https://www.wisslanding.com
*/

#include "filter/in.hpp"

#include <esp_attr.h>
#include <esp_log.h>
#include <string.h>

namespace ruth {

namespace filter {

static const char *TAG = "filter In";

In::In(const char *filter, const size_t len) : Split(len) {
  split(filter);

  // dump();
}

void In::dump() const {

  for (size_t i = 0; i < _level_count; i++) {
    ESP_LOGI(TAG, "level[%u] %s", i, _levels[i]);
  }

  ESP_LOGI(TAG, "length: %u", length());
}

} // namespace filter
} // namespace ruth
