
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

#pragma once

#include "ru_base/types.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <esp_log.h>
#include <iterator>
#include <tuple>

namespace ruth {
namespace desk {
namespace async {

//
// matcher uses the msgpack spec to find the beginning and end
// of a complete message by identifying the encoded representation
// of the 'mt' and 'ma' keys.
//
// https://github.com/msgpack/msgpack/blob/master/spec.md
//

class matcher {
public:
  inline explicit matcher() noexcept {}

  template <typename Iterator>
  inline std::pair<Iterator, bool> operator()(Iterator begin, Iterator end) const {
    const auto n = std::distance(begin, end);

    // first, do we have enough bytes to detect EOM?
    if (n < std::ssize(suffix)) return std::make_pair(begin, false);

    auto found = std::search(begin, end,                   // search sequence
                             suffix.begin(), suffix.end(), // for this pattern
                             [](const auto &in, const auto &s) {
                               // type cast in to be certain of equality
                               // (the interators passed point to char)
                               return static_cast<uint8_t>(in) == s;
                             });

    if (found == end) return std::make_pair(begin, false);

    return std::make_pair(found + std::ssize(suffix), true);
  }

private:
  bool partial{false};
  int partial_pos{0};
  // msgpack encoding of { "ma" = 828 }
  static constexpr std::array<uint8_t, 5> suffix{0x6d, 0x61, 0xcd, 0x03, 0x3c};
};
} // namespace async
} // namespace desk
} // namespace ruth

namespace asio {
template <> struct is_match_condition<ruth::desk::async::matcher> : public std::true_type {};
} // namespace asio