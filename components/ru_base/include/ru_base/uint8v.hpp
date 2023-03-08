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

#include <string_view>
#include <vector>

namespace ruth {

class uint8v : public std::vector<uint8_t> {
public:
  uint8v() = default;
  uint8v(size_t count, uint8_t val = 0x00) noexcept : std::vector<uint8_t>(count, val) {}
  ~uint8v() noexcept {} // prevent default copy/move

  uint8v(uint8v &&) = default;
  uint8v &operator=(uint8v &&) = default;

  template <typename T = const char> T *raw() const noexcept { return (T *)data(); }

  std::string_view view() const noexcept { return std::string_view(raw(), size()); }

private:
  static constexpr auto TAG{"uint8v"};
};

} // namespace ruth