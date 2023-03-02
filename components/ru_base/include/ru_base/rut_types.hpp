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

#include <chrono>
#include <type_traits>

namespace ruth {

using namespace std::chrono_literals;

using Micros = std::chrono::microseconds;
using Millis = std::chrono::milliseconds;
using Nanos = std::chrono::nanoseconds;
using Seconds = std::chrono::seconds;
using steady_clock = std::chrono::steady_clock;
using system_clock = std::chrono::system_clock;
using TimePoint = std::chrono::time_point<steady_clock>;

} // namespace ruth