/*
    Ruth
    Copyright (C) 2022  Tim Hughey

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

#include "ru_base/types.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace ruth {

class uint8v : public std::vector<uint8_t> {
public:
  uint8v() = default;
  uint8v(size_t count, uint8_t val) { assign(count, val); }
  uint8v(size_t reserve_default) : reserve_default(reserve_default) { //
    reserve(reserve_default);
  }

  uint8v(uint8v &src) = delete;                  // no copy assignment
  uint8v(const uint8v &src) = delete;            // no copy constructor
  uint8v &operator=(uint8v &rhs) = delete;       // no copy assignment
  uint8v &operator=(const uint8v &rhs) = delete; // no copy assignment

  uint8v(uint8v &&src) = default;
  uint8v &operator=(uint8v &&rhs) = default;

  bool multiLineString() const {
    auto nl_count = std::count_if(begin(), end(), [](const char c) { return c == '\n'; });

    return nl_count > 2;
  }

  template <typename T> const T *raw() const { return (const T *)data(); }
  void reset(size_t reserve_bytes = 0) {
    clear();

    if ((reserve_bytes == 0) && (reserve_default > 0)) {
      reserve(reserve_default);

    } else if (reserve_bytes > 0) {
      reserve(reserve_bytes);
    }
  }

  const string_view view() const { return string_view(raw<char>(), size()); }

  // debug, logging

  virtual void dump() const;
  // virtual string inspect() const;
  virtual csv moduleId() const { return module_id_base; }

protected:
  bool printable() const {
    if (size()) {
      return std::all_of( // only look at the first 10%
          begin(), begin() + (size() / 10),
          [](auto c) { return std::isprint(static_cast<unsigned char>(c)); });
    }

    return false;
  }
  string &toByteArrayString(string &msg) const;

private:
  size_t reserve_default = 0;
  static constexpr csv module_id_base{"UINT8V"};
};

} // namespace ruth