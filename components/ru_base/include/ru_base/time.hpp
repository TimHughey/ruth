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

#include "ru_base/helpers.hpp"
#include "ru_base/types.hpp"

#include <chrono>
#include <esp_timer.h>
#include <time.h>
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
using TimePoint = std::chrono::time_point<steady_clock>;

struct ru_time {
  static constexpr Nanos NS_FACTOR{upow(10, 9)};

  template <typename FROM, typename TO> static constexpr TO as_duration(auto x) {
    return std::chrono::duration_cast<TO>(FROM(x));
  }

  template <typename T> static constexpr MillisFP as_millis_fp(const T &d) {
    return std::chrono::duration_cast<MillisFP>(d);
  }

  template <typename T> static constexpr Seconds as_secs(const T &d) {
    return std::chrono::duration_cast<Seconds>(d);
  }

  template <typename T>
  static constexpr T elapsed_as(const Nanos &d1, const Nanos d2 = nowNanos()) {
    return std::chrono::duration_cast<T>(d2 - d1);
  }

  template <typename T = Micros, typename S = T>
  static constexpr T elapsed_abs(const T &d1, const S d2 = nowMicrosSystem()) {
    if (std::is_same<T, S>::value) {
      return std::chrono::abs(d2 - d1);
    } else {
      return std::chrono::abs(as_duration<S, T>(d2) - d1);
    }
  }

  static Nanos elapsed_abs_ns(const Nanos &d1,
                              const Nanos d2 = nowNanos()) { //
    return std::chrono::abs(d2 - d1);
  }

  static constexpr Millis from_ms(int64_t ms) { return Millis(ms); }
  static constexpr Nanos from_ns(uint64_t ns) { return Nanos(ns); }
  static constexpr Nanos negative(Nanos d) { return Nanos::zero() - d; }

  static Micros nowMicrosSystem() { return Micros(esp_timer_get_time()); }
  static Millis nowMillis() { return std::chrono::duration_cast<Millis>(nowNanos()); }

  static Nanos nowNanos() {
    struct timespec tn;
    clock_gettime(CLOCK_MONOTONIC, &tn);

    int64_t secs_part = tn.tv_sec * NS_FACTOR.count();
    int64_t ns_part = tn.tv_nsec;

    return Nanos(secs_part + ns_part);
  }

  template <typename T> static T nowSteady() {
    if (std::is_same<T, Micros>::value) {
      return Micros(esp_timer_get_time());
    }

    return as_duration<Nanos, T>(nowNanos());
  }
};

} // namespace ruth
