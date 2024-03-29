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

#ifndef ruth_in_filter_hpp
#define ruth_in_filter_hpp

#include <cstring>
#include <memory>

#include "filter/split.hpp"

namespace filter {

class In : public Split {
public:
  In(const char *filter, const size_t len);
  ~In() = default;

  void dump() const override;
};

} // namespace filter

#endif
