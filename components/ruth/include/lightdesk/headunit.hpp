/*
    devs/dmx/headunit.hpp - Ruth Dmx Head Unit Device
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

#ifndef _ruth_dmx_headunit_device_hpp
#define _ruth_dmx_headunit_device_hpp

#include "local/types.hpp"

namespace ruth {

typedef class HeadUnit HeadUnit_t;

class HeadUnit {
public:
  HeadUnit(const uint16_t address, size_t frame_len);
  virtual ~HeadUnit(){};
  virtual void framePrepare() {}
  void frameUpdate(uint8_t *frame_actual);

protected:
  inline bool &frameChanged() { return _frame_changed; }
  inline uint8_t *frameData() { return _frame_snippet; }

protected:
  uint16_t _address = 0;

private:
  bool _frame_changed = false;
  size_t _frame_len = 0;

  uint8_t _frame_snippet[10] = {};
};

} // namespace ruth

#endif
