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
#include <vector>

#include "local/types.hpp"

namespace ruth {

template <typename T, size_t CAP = 5> class MovingAverage {
  using vector = std::vector<T>;

public:
  MovingAverage() {
    _values.resize(CAP);
    _values.assign(1.0);
  };

  void addValue(T val) {

    // the _values vector contains a history of values constrained to the
    // initial capacity. the values are ordered by time with the latest
    // at the end and the oldest at the front.
    if (_values.size() < CAP) {
      // simply add values to _points until it reaches capacity
      _values.push_back(val);
    } else {
      // once capacity is reached, remove the first value then add the latest
      // to the end to prevent realloaction / resizing.
      _values.erase(_values.begin());
      _values.push_back(val);

      calculate();
    }
  }

  bool calculated() const { return _calculated; }

  T latest() {
    if (_calculated) {
      return _latest;
    } else {
      return 0;
    }
  }

private:
  void calculate() {
    _latest = 0;

    for (auto k = 0; k < _values.size(); k++) {
      _latest += _values[k];
    }

    _latest /= _values.size();
    _calculated = true;
  }

private:
  vector _values = {1};
  bool _calculated = false;

  T _latest;
};

} // namespace ruth
#endif
