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

#ifndef message_filter_hpp
#define message_filter_hpp

#include <memory>
#include <string_view>

namespace message {

//
// WARNING!!
//
// Filter currently supports inbound and outbound filters using a single memory area for the filter.
// this implies that array access of filter levels DOES NOT behave correctly for outbound filters!
//

class Filter {
public:
  Filter(char report = 0x00);
  Filter(const char *topic, const size_t len);
  Filter(const Filter &rhs);
  ~Filter() {}

  void append(const char *env);
  void appendHostId();
  void appendHostName();
  inline void appendMultiLevelWildcard() {
    addLevelSeparator();
    if (_capacity == 0) return;

    *_next++ = '#';
    _capacity--;
  }

  size_t availableCapacity() const { return _capacity; }
  const char *c_str() const { return _filter; }
  size_t length() const { return _next - _filter; }
  const char *level(size_t idx) const { return _levels[idx]; }
  const char *operator[](size_t idx) const { return _levels[idx]; }
  static void setFirstLevel(const char *level) { _first_level = level; };

private:
  void addLevel(const char *);
  inline void addLevelSeparator() {
    if (_capacity == 0) return;

    *_next++ = '/';
    _capacity--;
  }

  void appendLevel(const char *level);
  void dump();
  void split(const char *topic, const size_t length);

private:
  static constexpr size_t _max_capacity = 128;
  static const char *_first_level;
  char _filter[_max_capacity] = {};
  char _filter_sv[_max_capacity]; // no need to zero out, nulls are added by split
  char *_next = _filter;
  size_t _capacity = _max_capacity - 1;

  char *_levels[10] = {};
  size_t _level_count = 0;
};

} // namespace message

#endif
