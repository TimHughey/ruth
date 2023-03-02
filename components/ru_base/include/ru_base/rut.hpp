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

#include "ru_base/rut_types.hpp"
#include "ru_base/types.hpp"

#include <esp_timer.h>
#include <type_traits>

namespace ruth {

struct rut {

  template <typename TO, typename FROM = Nanos> static constexpr TO as(FROM x) noexcept {
    if constexpr (std::same_as<TO, FROM>) {
      return x;
    } else {
      return std::chrono::duration_cast<TO>(x);
    }
  }

  template <typename AS, typename BASE = AS, typename T>
  static constexpr AS from_val(T val) noexcept {

    if constexpr (std::is_floating_point_v<T>) {
      // round the value
      int64_t count = static_cast<int64_t>(val);
      return as<AS, BASE>(BASE(count));
    } else if constexpr (std::is_integral_v<T>) {
      int64_t count = static_cast<int64_t>(val);
      return as<AS, BASE>(BASE(count));
    } else if constexpr (std::is_convertible_v<T, BASE>) {
      return as<AS, BASE>(BASE(val));
    } else {
      static_assert(always_false_v<T>, "unsupported type");
    }
  }

  template <typename T> static inline T now_epoch() {
    return std::chrono::duration_cast<T>(system_clock::now().time_since_epoch());
  }

  static inline Micros raw() noexcept { return Micros(esp_timer_get_time()); }
  static inline int64_t raw_us() noexcept { return esp_timer_get_time(); }
};

} // namespace ruth
