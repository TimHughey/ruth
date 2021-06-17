/*
  Ruth
  (C)opyright 2021  Tim Hughey

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

#ifndef ruth_filter_split_hpp
#define ruth_filter_split_hpp

#include <cstring>
#include <memory>

#include "filter/filter.hpp"

namespace filter {

class Split : public Filter {
public:
  Split(const size_t len);
  virtual ~Split() = default;

  virtual void dump() const override = 0;
  size_t length() const { return _length; }
  const char *level(size_t idx) const { return _levels[idx]; }
  const char *operator[](size_t idx) const { return _levels[idx]; }

protected:
  void split(const char *filter);

protected:
  const size_t _length;

  char *_levels[10] = {};
  size_t _level_count = 0;
};

} // namespace filter

#endif
