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
  inline uint8v() = default;
  inline uint8v(size_t count, uint8_t val = 0x00) noexcept { assign(count, val); }

  uint8v(uint8v &src) = delete;                  // no copy assignment
  uint8v(const uint8v &src) = delete;            // no copy constructor
  uint8v &operator=(uint8v &rhs) = delete;       // no copy assignment
  uint8v &operator=(const uint8v &rhs) = delete; // no copy assignment

  inline uint8v(uint8v &&src) = default;
  inline uint8v &operator=(uint8v &&rhs) = default;

  template <typename T = char> inline const T *raw() const { return (const T *)data(); }

  const string_view view() const { return string_view(raw<char>(), size()); }

  // virtual string inspect() const;
  virtual csv moduleId() const { return module_id_base; }

private:
  static constexpr csv module_id_base{"UINT8V"};
};

} // namespace ruth