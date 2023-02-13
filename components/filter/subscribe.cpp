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

#include "filter/subscribe.hpp"

#include <esp_attr.h>
#include <esp_log.h>
#include <string.h>

namespace ruth {

namespace filter {

static const char *TAG = "filter Subscribe";

Subscribe::Subscribe(const char *first_level) : Builder(first_level) {
  addLevel("c2");
  addHostId();
  addChar('#');
}

void Subscribe::dump() const { ESP_LOGI(TAG, "%s", c_str()); }

} // namespace filter
} // namespace ruth
