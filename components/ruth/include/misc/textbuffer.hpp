/*
    textbuffer.hpp -- Static Text Buffer
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

#ifndef _ruth_text_buffer_hpp
#define _ruth_text_buffer_hpp

#include <cstdio>
#include <cstdlib>
#include <stdarg.h>
#include <string.h>

namespace ruth {

template <size_t CAP = 15> class TextBuffer {
public:
  TextBuffer(){};
  TextBuffer(const char *str) {
    _size = strnlen_s(str);
    strncpy(_buff, str, CAP);
  }

  TextBuffer(const char *str, size_t len) {
    _size = (len < CAP) ? len : CAP;
    memcpy(_buff, str, _size);
  }

  void assign(const char *str) {
    _size = strnlen(str, CAP);
    strncpy(_buff, str, _size);
  }

  void assign(const char *str, size_t len) {
    _size = (len < CAP) ? len : CAP;
    memcpy(_buff, str, _size);
  }

  void assign(const uint8_t *start, const uint8_t *end) {
    size_t len = end - start;
    _size = (len < CAP) ? len : CAP;
    memcpy(_buff, start, _size);
  }

  void calcSize() { _size = strlen_s(_buff); }
  size_t capacity() const { return CAP; }
  void clear() {
    _size = 0;
    bzero(_buff, CAP);
  }

  int compare(const char *str) const { return strncmp(_buff, str, CAP); }

  const char *c_str() const { return _buff; }
  char *data() { return _buff; }
  bool empty() const { return ((_size == 0) ? true : false); }
  void forceSize(size_t size) { _size = (size > CAP) ? CAP : size; }
  size_t length() const { return _size; }
  bool match(const char *str) const {
    return (compare(str) == 0) ? true : false;
  }

  void printf(const char *format, ...) {
    va_list arglist;

    va_start(arglist, format);
    _printf(format, arglist);
    va_end(arglist);
  }

  void printf(const char *format, va_list arglist) { _printf(format, arglist); }

  size_t size() const { return _size; }

  TextBuffer &operator=(const char *rhs) {
    copySafe(rhs);

    return *this;
  }

  float usedPrecent() const { return ((float)_size / (float)CAP) * 100; }

  bool operator==(const char *str) const {
    return (compare(str) == 0) ? true : false;
  }

  // support type casting from DeviceAddress to a plain ole const char *
  operator const char *() { return _buff; };

private:
  void copySafe(const char *str) {
    strncpy(_buff, str, CAP);

    if (_size > CAP) {
      _size = CAP;
      _buff[CAP - 1] = 0x00;
    } else {
      _size = strnlen(str, CAP);
    }
  }

  void _printf(const char *format, va_list arglist) {
    const size_t cap = CAP - _size;
    auto bytes = vsnprintf((_buff + _size), cap, format, arglist);
    _size += bytes;
  }

  size_t strnlen_s(const char *str) const {
    size_t i = 0;

    for (i = 0; (i < CAP) && str[i]; i++) {
      // continue through array
    }

    return i;
  }

private:
  char _buff[CAP + 1] = {};
  size_t _size = 0;
};

} // namespace ruth
#endif
