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

#include "filter/filter.hpp"

#include <esp_attr.h>
#include <string.h>

namespace ruth {
namespace filter {

const char *Filter::_first_level = nullptr;
const char *Filter::_host_id = nullptr;
const char *Filter::_hostname = nullptr;

void Filter::init(const Opts &opts) {
  _first_level = opts.first_level;
  _host_id = opts.host_id;
  _hostname = opts.hostname;
}

} // namespace filter
} // namespace ruth
