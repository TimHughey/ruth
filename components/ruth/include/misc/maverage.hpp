/*
    misc/maverage.hpp -- Moving Average
    Copyright (C) 2021  Tim Hughey

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
*/

#ifndef _ruth_moving_average_hpp
#define _ruth_moving_average_hpp

#include <cstdint>

#include "local/types.hpp"
#include "misc/elapsed.hpp"

namespace ruth {

template <typename T, uint32_t SECONDS> class MovingAverage {

public:
  MovingAverage() { _avg_elapsed.reset(); };

  void addValue(T val) {
    constexpr uint32_t ms = SECONDS * 1000;

    if ((uint32_t)_avg_elapsed <= ms) {
      _sum += val;
      _count++;

      _latest = _sum / _count;
    } else {
      _sum = _latest;
      _count = 1;

      _avg_elapsed.reset();
    }
  }

  T latest() const { return _latest; }

private:
  elapsedMillis _avg_elapsed;

  double _sum = 0;
  uint64_t _count = 0;

  T _latest = 0;
};

} // namespace ruth
#endif
