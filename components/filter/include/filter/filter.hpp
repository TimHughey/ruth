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

#ifndef ruth_filter_hpp
#define ruth_filter_hpp

#include <memory>

namespace filter {

struct Opts {
  const char *first_level = nullptr;
  const char *host_id = nullptr;
  const char *hostname = nullptr;
};

class Filter {

public:
  Filter() {}
  virtual ~Filter() {}

  const char *c_str() const { return _filter; }
  virtual void dump() const = 0;
  static void init(const Opts &opts);
  virtual size_t length() const = 0;

protected:
  const char *hostID() const { return _host_id; };
  const char *hostname() const { return _hostname; };

protected:
  static const char *_first_level;
  static const char *_host_id;
  static const char *_hostname;

protected:
  static constexpr size_t _max_capacity = 128;
  char _filter[_max_capacity] = {};
};
} // namespace filter

#endif
