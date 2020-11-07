/*
    devs/i2c/rawdata.hpp - Stack Allocated Raw Data Buffer
    Copyright (C) 2017  Tim Hughey

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

#ifndef _ruth_i2c_rawdata_hpp
#define _ruth_i2c_rawdata_hpp

#include <initializer_list>
#include <memory>

using std::initializer_list;

namespace ruth {

typedef class RawData RawData_t;

class RawData {
public:
  RawData(){};
  RawData(const initializer_list<uint8_t> list) { copyList(list); }

  inline uint8_t at(size_t index) const { return data_[index]; }
  inline size_t capacity() const { return capacity_; }
  inline void clear() { size_ = 0; }
  inline uint8_t *data() { return data_; }
  inline size_t reserve(size_t bytes) { return setSize(bytes); }
  inline size_t resize(size_t bytes) { return setSize(bytes); }
  inline uint8_t size() const { return size_; }

  inline RawData &operator=(const initializer_list<uint8_t> list) {
    copyList(list);
    return *this;
  }

  inline uint8_t operator[](size_t idx) const { return data_[idx]; };
  // uint8_t *operator[](size_t idx) { return &(data_[idx]); };

private:
  static const size_t capacity_ = 15;
  uint8_t data_[capacity_] = {};
  size_t size_ = 0;

  inline void copyList(const initializer_list<uint8_t> &list) {
    size_t index = 0;

    setSize(list.size());

    for (uint8_t val : list) {
      if (index < size_) {
        data_[index] = val;
      }
      index++;
    }
  }

  inline size_t setSize(size_t bytes) {
    size_ = (bytes < capacity_) ? bytes : capacity_;
    return capacity_;
  }
};
} // namespace ruth

#endif
