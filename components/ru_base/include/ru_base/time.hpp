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

#include "ru_base/helpers.hpp"
#include "ru_base/types.hpp"

#include <chrono>
#include <esp_timer.h>
#include <type_traits>

namespace ruth {

using namespace std::chrono_literals;

using Micros = std::chrono::microseconds;
using MicrosFP = std::chrono::duration<double, std::chrono::microseconds::period>;
using Millis = std::chrono::milliseconds;
using MillisFP = std::chrono::duration<double, std::chrono::milliseconds::period>;
using Nanos = std::chrono::nanoseconds;
using Seconds = std::chrono::duration<double>;
using steady_clock = std::chrono::steady_clock;
using system_clock = std::chrono::system_clock;
using TimePoint = std::chrono::time_point<steady_clock>;

struct ru_time;
using rut = ru_time;

struct ru_time {
  static constexpr Nanos NS_FACTOR{upow(10, 9)};

  template <typename FROM, typename TO> static inline constexpr TO as_duration(auto x) {
    return std::chrono::duration_cast<TO>(FROM(x));
  }

  template <typename T> static constexpr inline MillisFP as_millis_fp(const T &d) {
    return std::chrono::duration_cast<MillisFP>(d);
  }

  template <typename T> static constexpr inline Seconds as_secs(const T &d) {
    return std::chrono::duration_cast<Seconds>(d);
  }

  template <typename T = Micros, typename S = T>
  static constexpr inline T elapsed_abs(const T &d1, const S d2 = rut::raw()) {
    if (std::is_same<T, S>::value) {
      return std::chrono::abs(d2 - d1);
    } else {
      return std::chrono::abs(as_duration<S, T>(d2) - d1);
    }
  }

  static constexpr inline Millis from_ms(int64_t ms) { return Millis(ms); }

  template <typename T> static inline T now_epoch() {
    return std::chrono::duration_cast<T>(system_clock::now().time_since_epoch());
  }

  static inline Micros raw() { return Micros(esp_timer_get_time()); }
  static inline int64_t raw_us() { return esp_timer_get_time(); }
};

} // namespace ruth
