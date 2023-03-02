//  Ruth
//  Copyright (C) 2020  Tim Hughey
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
//
//  Based on the original work of:
//    http://www.pjrc.com/teensy/
//    Copyright (c) 2011 PJRC.COM, LLC

#pragma once

#include "ru_base/rut_types.hpp"

#include <concepts>
#include <cstdint>
#include <esp_timer.h>
namespace ruth {

class Elapsed {
private:
  // early definition for auto
  template <typename T> inline auto elapsed() const noexcept {
    const auto val_now = frozen ? val : esp_timer_get_time() - val;

    if constexpr (std::same_as<T, Millis>) {
      return Millis(val_now / 1000);
    } else if constexpr (std::same_as<T, Micros>) {
      return Micros(val_now);
    } else if constexpr (std::same_as<T, int64_t>) {
      return val_now;
    } else {
      static_assert(true, "unsupported type");
    }
  }

public:
  inline Elapsed(void) noexcept : val(esp_timer_get_time()), frozen{false} {}

  inline int64_t operator()() noexcept { return elapsed<int64_t>(); }

  template <typename T> inline auto as() const noexcept { return elapsed<T>(); }

  inline const auto freeze() noexcept {
    if (!frozen) {
      const auto frozen_val = elapsed<int64_t>();
      frozen = true;
      return frozen_val;
    }

    return val;
  }

  template <typename T> inline bool operator>=(const T &rhs) const noexcept {
    return elapsed<T>() >= rhs;
  }

  inline Elapsed &reset() noexcept {
    *this = Elapsed();
    return *this;
  }

private:
  int64_t val;
  bool frozen;
};

} // namespace ruth
