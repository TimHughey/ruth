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
#include <deque>

#include "local/types.hpp"
#include "misc/elapsed.hpp"

namespace ruth {

template <typename T, uint32_t SECONDS> class MovingAverage {

  typedef std::deque<T> maDeque_t;

public:
  MovingAverage() { _e.reset(); };

  void addValue(T val) {
    if (_first_add) {
      _e.reset();
      _first_add = false;
    }

    if (_e.toSeconds() <= SECONDS) {
      // add to container until we've the duration requested
      _dq.emplace_back(val);
    } else {
      // we now know the total number of items that represent the requested
      // duration (assuming frequency of adding values is a constant).
      // instead of adding more values we remove the oldest (at the front) and
      // add the new (to the back)
      _dq.pop_front();
      _dq.emplace_back(val);
    }
  }

  T latest() const { return calculate(); }
  T lastValue() const {
    T val = 0;
    if (_dq.empty() == false) {
      val = _dq.back();
    }
    return val;
  }

  size_t size() const { return _dq.size(); }

private:
  T calculate() const {
    T sum = 0;

    for (const T &val : _dq) {
      sum += val;
    }

    return sum / (T)_dq.size();
  }

private:
  bool _first_add = true;
  elapsedMillis _e;

  maDeque_t _dq = {};
};

} // namespace ruth
#endif
