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

#include "crc.hpp"
#include "owb/owb.h"

namespace ds {

IRAM_ATTR bool checkCrc16(const Crc16Opts &opts) {
  const uint16_t crc_calc = ~crc16(opts);
  return (crc_calc & 0xff) == opts.inverted[0] && (crc_calc >> 8) == opts.inverted[1];
}

IRAM_ATTR uint16_t crc8(const uint8_t *bytes, const size_t len) { return owb_crc8_bytes(0x00, bytes, len); }

IRAM_ATTR uint16_t crc16(const Crc16Opts &opts) {
  static const uint8_t oddparity[16] = {0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0};

  uint16_t crc_calc = opts.crc >> 8;
  for (size_t i = 0; i < opts.len; i++) {
    // we copy a single byte from the input however perform 16-bit computation
    uint16_t cdata = (opts.input[i] ^ opts.crc) & 0xff;

    if (oddparity[cdata & 0x0f] ^ oddparity[cdata >> 4]) crc_calc ^= 0xc001;

    cdata <<= 6;
    crc_calc ^= cdata;
    cdata <<= 1;
    crc_calc ^= cdata;
  }

  return crc_calc;
}
} // namespace ds
