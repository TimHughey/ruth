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

#ifndef ruth_dev_ds_crc_hpp
#define ruth_dev_ds_crc_hpp

#include <memory>

namespace ds {

// struct Crc16Opts {
//   const uint8_t *input;
//   const size_t len;
//   const uint8_t *inverted;
//   const uint16_t crc;
// };
//
// bool checkCrc16(const Crc16Opts &opts);
bool checkCrc16(const uint8_t *start, const uint8_t *end);
uint16_t crc8(const uint8_t *bytes, const size_t len);
// uint16_t crc16(const Crc16Opts &opts);

} // namespace ds
#endif
