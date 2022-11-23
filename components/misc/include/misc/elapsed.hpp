/*
    misc/elapsed.hpp - Ruth Elapsed Time Measurement
    Copyright (C) 2020  Tim Hughey

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

    Based on the original work of:
      http://www.pjrc.com/teensy/
      Copyright (c) 2011 PJRC.COM, LLC
*/

#pragma once

#include "ru_base/time.hpp"

#include <cstdint>
#include <esp_timer.h>
namespace ruth {

class Elapsed {
public:
  inline Elapsed(void) noexcept : val(rut::raw()), frozen{false} {}

  auto operator()() { return elapsed(); }

  template <typename T> inline T as() const noexcept {
    return rut::as_duration<Nanos, T>(elapsed());
  }

  inline const auto freeze() noexcept {
    if (!frozen) {
      frozen = true;
      val = rut::raw() - val;
    }

    return val;
  }

  template <typename T> inline bool operator>=(const T &rhs) const noexcept {
    return elapsed() >= rhs;
  }

  inline Elapsed &reset() noexcept {
    *this = Elapsed();
    return *this;
  }

private:
  inline Micros elapsed() const noexcept { return frozen ? val : rut::raw() - val; }

private:
  Micros val;
  bool frozen;
};

} // namespace ruth
