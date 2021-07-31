/*
  Message
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
#ifndef message_in_hpp
#define message_in_hpp

#include <memory.h>

#include "ArduinoJson.h"

#include "filter/in.hpp"

namespace message {

class In {
public:
  In(const char *filter, const size_t filter_len, const char *packed, const size_t packed_len);
  ~In() {}

  inline const char *category() const { return filter(3); }
  inline const char *filter(const uint32_t idx) const { return _filter[idx]; }
  inline const char *hostnameFromFilter() const { return filter(5); }
  inline const char *identFromFilter() const { return filter(4); }
  inline const char *kindFromFilter() const { return filter(4); }
  inline uint32_t kind() const { return _kind; }

  static std::unique_ptr<In> make(const char *filter, const size_t filter_len, const char *packed,
                                  const size_t packed_len);

  inline const char *refidFromFilter() const { return filter(5); }

  bool unpack(JsonDocument &doc);
  bool valid() const { return _valid; }
  inline void want(uint32_t kind) { _kind = kind; }
  bool wanted() const { return _kind > 0; }

private:
  void checkTime(JsonDocument &root);

private:
  filter::In _filter;
  uint32_t _kind = 0;
  size_t _packed_len;
  std::unique_ptr<char[]> _packed;
  bool _valid = false;
  DeserializationError _err;
};

typedef std::unique_ptr<In> InWrapped;
} // namespace message

#endif
