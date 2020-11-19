/*
    datetime.hpp - Ruth DateTime
    Copyright (C) 2020  Tim Hughey

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

#ifndef ruth_datetime_hpp
#define ruth_datetime_hpp

#include <sys/time.h>
#include <time.h>

#include "textbuffer.hpp"

namespace ruth {

typedef class DateTime DateTime_t;

class DateTime {
public:
  DateTime(time_t t = 0);
  const char *get() const { return buffer_.c_str(); };

private:
  TextBuffer<25> buffer_;
};
} // namespace ruth

#endif
