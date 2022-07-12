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

#include "filter/split.hpp"

#include <esp_attr.h>
#include <string.h>

namespace ruth {

namespace filter {

IRAM_ATTR Split::Split(const size_t len) : _length(len) {}

IRAM_ATTR void Split::split(const char *filter) {
  // 1. make a copy of the event topic (filter) and null terminate
  // 2. search the filter for the level separator
  // 3. when each level separator is found record the starting pointer and replace
  //    the '/' with a null to terminate the string
  // 4. at the conclusion we have an array of pointers to the filter levels within the
  //    copy of the event topic

  memccpy(_filter, filter, 0x00, _max_capacity);
  _filter[_length] = 0x00;

  // set search used to find levels 0..4 (max of 5)
  auto search = _filter;
  // end points to the added null terminator
  auto const end = search + _length;
  constexpr size_t max_levels = sizeof(_levels) / sizeof(char *);

  // find each separator, record the found level and null the separator to
  // to create each individual string
  while ((_level_count < max_levels) && (search < end)) {
    auto separator = strchr(search, '/');

    // record the found level if it's not the end of the filter
    if (search != 0x00) {
      _levels[_level_count++] = search;
    }

    // reached the end of the filter, stop searching
    if (separator == nullptr)
      break;

    // null the separator, advance search to continue finding levels
    *separator = 0x00;
    search += separator - search + 1;
  }
}

} // namespace filter
} // namespace ruth
