/*
    misc/derivative.hpp -- Derivative (rate of change)
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

#ifndef _ruth_derivative_hpp
#define _ruth_derivative_hpp

#include <cstdint>
#include <vector>

#include "local/types.hpp"

namespace ruth {

template <typename T, size_t CAP = 5> class Derivative {
  using vector = std::vector<T>;

public:
  Derivative() { _points.resize(CAP); };

  void addValue(T val) {

    // the _points vector contains a history of values constrained to the
    // initial capacity. the values are ordered by time with the latest
    // at the end and the oldest at the front.
    if (_points.size() < CAP) {
      // simply add values to _points until it reaches capacity
      _points.push_back(val);
    } else {
      // once capacity is reached, remove the first value then add the latest
      // to the end to prevent realloaction / resizing.
      _points.erase(_points.begin());
      _points.push_back(val);
      calculate();
    }
  }

  bool calculated() const { return _calculated; }

  T rateOfChange() {
    if (_calculated) {
      return _rate_of_change;
    } else {
      return 0;
    }
  }

private:
  void calculate() {
    _rate_of_change = 0;

    for (auto k = 0; k < _num_coeff; k++) {
      _rate_of_change += _points[k] * static_cast<T>(_coeff[k]);
    }

    _rate_of_change /= CAP;
    _calculated = true;
  }

private:
  vector _points;

  const int_fast16_t _coeff[5] = {1, -8, 0, 8, -1};
  static constexpr size_t _num_coeff = sizeof(_coeff) / sizeof(int_fast16_t);

  bool _calculated = false;

  T _rate_of_change;
};

} // namespace ruth
#endif
