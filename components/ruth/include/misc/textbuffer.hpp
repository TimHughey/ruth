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
    size_ = strnlen_s(str);
    strncpy(buff_, str, CAP);
  }

  TextBuffer(const char *str, size_t len) {
    size_ = (len < CAP) ? len : CAP;
    memcpy(buff_, str, size_);
  }

  void assign(const char *str, size_t len) {
    size_ = (len < CAP) ? len : CAP;
    memcpy(buff_, str, size_);
  }

  void assign(const uint8_t *start, const uint8_t *end) {
    size_t len = end - start;
    size_ = (len < CAP) ? len : CAP;
    memcpy(buff_, start, size_);
  }

  void calcSize() { size_ = strlen_s(buff_); }
  size_t capacity() const { return CAP; }
  void clear() {
    size_ = 0;
    bzero(buff_, CAP);
  }

  int compare(const char *str) const { return strncmp(buff_, str, CAP); }

  const char *c_str() const { return buff_; }
  char *data() { return buff_; }
  bool empty() const { return ((size_ == 0) ? true : false); }
  void forceSize(size_t size) { size_ = (size > CAP) ? CAP : size; }
  size_t length() const { return size_; }
  bool match(const char *str) const {
    return (compare(str) == 0) ? true : false;
  }

  void printf(const char *format, ...) {
    va_list arglist;

    va_start(arglist, format);
    size_ = vsnprintf(buff_, CAP, format, arglist);
    va_end(arglist);
  }

  void printf(const char *format, va_list arglist) {
    size_ = vsnprintf(buff_, CAP, format, arglist);
  }

  size_t size() const { return size_; }

  TextBuffer &operator=(const char *rhs) {
    size_ = strnlen_s(rhs);
    copySafe(rhs);

    return *this;
  }

  float usedPrecent() const { return ((float)size_ / (float)CAP) * 100; }

  bool operator==(const char *str) const {
    return (compare(str) == 0) ? true : false;
  }

  // support type casting from DeviceAddress to a plain ole const char *
  operator const char *() { return buff_; };

private:
  void copySafe(const char *str) {
    strncpy(buff_, str, CAP);

    if (size_ > CAP) {
      size_ = CAP;
      buff_[CAP - 1] = 0x00;
    }
  }

  size_t strnlen_s(const char *str) const {
    size_t i = 0;

    for (i = 0; (i < CAP) && str[i]; i++) {
      // continue through array
    }

    return i;
  }

private:
  char buff_[CAP + 1] = {};
  size_t size_ = 0;
};

} // namespace ruth
#endif
