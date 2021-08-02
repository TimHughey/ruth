/*
    Ruth
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

#ifndef misc_datetime_hpp
#define misc_datetime_hpp

#include <sys/time.h>
#include <time.h>

#include "textbuffer.hpp"

typedef class DateTime DateTime_t;

class DateTime {
public:
  DateTime(time_t t = 0, const char *format = "%c");
  const char *c_str() const { return _buffer; }
  static uint64_t now();

private:
  static constexpr size_t _buff_len = 25;
  char _buffer[_buff_len];
};

#endif
