/*
    devs/pinspot]/base.cpp - Ruth Pinspot Device
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

#include "lightdesk/headunit.hpp"

namespace ruth {

HeadUnit::HeadUnit(uint16_t address, size_t frame_len)
    : _address(address), _frame_len(frame_len) {}

void IRAM_ATTR HeadUnit::frameUpdate(uint8_t *frame_actual) {

  // HeadUnit subclasses that require their state to be reflected
  // in the DMX frame will have set _frame_len > 0
  if (_frame_changed && (_frame_len > 0)) {
    bcopy(_frame_snippet, (frame_actual + _address), _frame_len);

    _frame_changed = false;
  } else if (_frame_len == 0) {
    // this is a pseudo headunit that is not included in the DMX frame
    // so make no changes to the frame however be certain _frame_changed
    // is set to false
    _frame_changed = false;
  }
}

} // namespace ruth
