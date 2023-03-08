
//  Pierre - Custom Light Show for Wiss Landing
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

#include "io/io.hpp"
#include "ru_base/types.hpp"

#include <asio/read_until.hpp>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ruth {
namespace desk {
namespace async {

class match_char {
public:
  explicit match_char(char c) : c_(c) {}

  template <typename Iterator>
  std::pair<Iterator, bool> operator()(Iterator begin, Iterator end) const {
    Iterator i = begin;
    while (i != end)
      if (c_ == *i++) return std::make_pair(i, true);
    return std::make_pair(i, false);
  }

private:
  char c_;
};
} // namespace async
} // namespace desk
} // namespace ruth

namespace asio {
template <> struct is_match_condition<ruth::desk::async::match_char> : public std::true_type {};
} // namespace asio