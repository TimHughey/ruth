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

#pragma once

#include "filter/filter.hpp"

#include <memory>

namespace ruth {

namespace filter {

class Builder : public Filter {
public:
  Builder(const char *first_level = nullptr);
  virtual ~Builder() = default;

  void addChar(const char c, bool with_separator = true);
  void addHostId() { addLevel(_host_id); }
  void addHostName() { addLevel(_hostname); }
  void addLevel(const char *, bool with_separator = true);
  inline void addLevelSeparator() { addChar('/', false); }

  size_t availableCapacity() const { return _capacity; }
  virtual void dump() const override = 0;
  size_t length() const override { return _next - _filter; }

protected:
  char *_next = _filter;
  size_t _capacity = _max_capacity - 1;
};

} // namespace filter
} // namespace ruth
