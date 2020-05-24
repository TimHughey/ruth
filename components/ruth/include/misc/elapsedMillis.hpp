/* Elapsed time types - for easy-to-use measurements of elapsed time
 * http://www.pjrc.com/teensy/
 * Copyright (c) 2011 PJRC.COM, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _ruth_elapsed_millis_hpp
#define _ruth_elapsed_millis_hpp

#include <cstdint>

#include <esp_timer.h>

namespace ruth {

class elapsedMillis {
private:
  uint64_t _ms;
  bool _frozen = false;

  inline uint64_t millis() const { return (esp_timer_get_time() / 1000); };
  inline uint64_t val() const { return (_frozen) ? (_ms) : (millis() - _ms); }

public:
  elapsedMillis(void) { _ms = millis(); }
  elapsedMillis(const elapsedMillis &orig)
      : _ms(orig._ms), _frozen(orig._frozen) {}

  void freeze(uint32_t val = UINT32_MAX) {
    _frozen = true;
    _ms = (val == UINT32_MAX) ? (millis() - _ms) : val;
  }

  void reset() {
    _frozen = false;
    _ms = millis();
  }
  operator uint64_t() const { return val(); }
  operator uint32_t() const { return val(); }
  operator float() const { return toSeconds(val()); }
  elapsedMillis &operator=(const elapsedMillis &rhs) {
    _ms = rhs._ms;
    return *this;
  }

  elapsedMillis &operator=(uint64_t val) {
    _ms = millis() - val;
    return *this;
  }
  elapsedMillis &operator-=(uint64_t val) {
    _ms += val;
    return *this;
  }
  elapsedMillis &operator+=(uint64_t val) {
    _ms -= val;
    return *this;
  }
  elapsedMillis operator-(int val) const {
    elapsedMillis r(*this);
    r._ms += val;
    return r;
  }
  elapsedMillis operator-(unsigned int val) const {
    elapsedMillis r(*this);
    r._ms += val;
    return r;
  }
  elapsedMillis operator-(long val) const {
    elapsedMillis r(*this);
    r._ms += val;
    return r;
  }
  elapsedMillis operator-(uint64_t val) const {
    elapsedMillis r(*this);
    r._ms += val;
    return r;
  }
  elapsedMillis operator+(int val) const {
    elapsedMillis r(*this);
    r._ms -= val;
    return r;
  }
  elapsedMillis operator+(unsigned int val) const {
    elapsedMillis r(*this);
    r._ms -= val;
    return r;
  }
  elapsedMillis operator+(long val) const {
    elapsedMillis r(*this);
    r._ms -= val;
    return r;
  }
  elapsedMillis operator+(uint64_t val) const {
    elapsedMillis r(*this);
    r._ms -= val;
    return r;
  }

  bool operator<(const elapsedMillis &rhs) const {
    elapsedMillis lhs(*this);
    return lhs.val() < rhs.val();
  }

  bool operator<(uint64_t rhs) const {
    elapsedMillis lhs(*this);

    return (lhs.val() < rhs) ? true : false;
  }

  static float toSeconds(uint64_t val) { return (float)val / 1000.0; }
};

class elapsedMicros {
private:
  uint64_t _us;
  bool _frozen = false;

  inline uint64_t micros() const { return (esp_timer_get_time()); };
  inline uint64_t val() const { return (_frozen) ? (_us) : (micros() - _us); }

public:
  elapsedMicros(void) : _us(micros()) {}
  elapsedMicros(const elapsedMicros &orig)
      : _us(orig._us), _frozen(orig._frozen) {}

  void freeze() {
    _frozen = true;
    _us = val();
  }
  void reset() {
    _frozen = false;
    _us = micros();
  }
  operator float() const { return toSeconds(val()); }
  operator uint64_t() const { return (_frozen) ? (_us) : (micros() - _us); }
  operator uint32_t() const { return (_frozen) ? (_us) : (micros() - _us); }
  elapsedMicros &operator=(const elapsedMicros &rhs) {
    _us = rhs._us;
    _frozen = rhs._frozen;
    return *this;
  }
  elapsedMicros &operator=(uint64_t val) {
    _us = micros() - val;
    return *this;
  }
  elapsedMicros &operator-=(uint64_t val) {
    _us += val;
    return *this;
  }
  elapsedMicros &operator+=(uint64_t val) {
    _us -= val;
    return *this;
  }
  elapsedMicros operator-(int val) const {
    elapsedMicros r(*this);
    r._us += val;
    return r;
  }
  elapsedMicros operator-(unsigned int val) const {
    elapsedMicros r(*this);
    r._us += val;
    return r;
  }
  elapsedMicros operator-(long val) const {
    elapsedMicros r(*this);
    r._us += val;
    return r;
  }
  elapsedMicros operator-(uint64_t val) const {
    elapsedMicros r(*this);
    r._us += val;
    return r;
  }
  elapsedMicros operator+(int val) const {
    elapsedMicros r(*this);
    r._us -= val;
    return r;
  }
  elapsedMicros operator+(unsigned int val) const {
    elapsedMicros r(*this);
    r._us -= val;
    return r;
  }
  elapsedMicros operator+(long val) const {
    elapsedMicros r(*this);
    r._us -= val;
    return r;
  }
  elapsedMicros operator+(uint64_t val) const {
    elapsedMicros r(*this);
    r._us -= val;
    return r;
  }

  static float toSeconds(uint64_t val) { return (float)val / 1000000.0; }
};

} // namespace ruth

#endif // elapsedMillis_h
