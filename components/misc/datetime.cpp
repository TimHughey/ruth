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

#include <esp_attr.h>

#include "misc/datetime.hpp"

DateTime::DateTime(time_t t, const char *format) {
  time_t mtime = (t == 0) ? time(nullptr) : t;

  struct tm timeinfo = {};
  localtime_r(&mtime, &timeinfo);

  strftime(_buffer, _buff_len, format, &timeinfo);
}

IRAM_ATTR uint64_t DateTime::now() {
  struct timeval time_now;

  uint64_t us_since_epoch;

  gettimeofday(&time_now, nullptr);

  us_since_epoch = 0;
  us_since_epoch += time_now.tv_sec * 1000000L; // convert seconds to microseconds
  us_since_epoch += time_now.tv_usec;           // add microseconds since last second

  return us_since_epoch;
}
