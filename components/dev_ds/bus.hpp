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

#ifndef ruth_ds_bus_hpp
#define ruth_ds_bus_hpp

#include <memory>

namespace ds {
typedef uint8_t Byte;
typedef uint8_t *Bytes;
typedef const size_t Len;
typedef uint8_t *RomCode;

class Bus {
public:
  static bool acquire(uint32_t timeout_ms = UINT32_MAX);
  static bool ensure();
  static bool error();

  static bool convert(bool &complete, bool cancel = false);
  static uint8_t lastStatus();
  static bool ok();
  static bool release();
  static bool reset();
  static bool search(RomCode rom_code);

  static bool writeThenRead(Bytes write, Len wlen, Bytes read, Len rlen);
  static bool writeThenRead(Bytes write, Len wlen, Byte read);

private:
  static void checkPowered();
};
} // namespace ds

#endif
