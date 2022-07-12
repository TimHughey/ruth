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

#include "filter/builder.hpp"

#include <esp_attr.h>
#include <esp_log.h>
#include <string.h>

namespace ruth {

namespace filter {

IRAM_ATTR Builder::Builder(const char *first_level) {
  if (first_level) {
    addLevel(first_level, false);
  } else {
    addLevel(_first_level, false);
  }
}

IRAM_ATTR void Builder::addChar(const char c, bool with_separator) {
  if (_capacity > 2) {

    if (with_separator)
      addLevelSeparator();

    *_next++ = c;
    _capacity--;
  }
}

IRAM_ATTR void Builder::addLevel(const char *filter, bool with_separator) {
  if (_capacity == 0)
    return;

  if (with_separator)
    addLevelSeparator();

  char *p = _next;
  p = (char *)memccpy(p, filter, 0x00, _capacity);

  p--;

  // p-1 because memccpy returns a pointer to just after the copied null
  // reduce capacity by what was placed into filter
  _capacity -= p - _next;
  _next = p;
}

} // namespace filter
} // namespace ruth
