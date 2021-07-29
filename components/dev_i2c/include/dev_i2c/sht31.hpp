/*
    Ruth
    Copyright (C) 2021  Tim Hughey

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

#ifndef ruth_dev_sht31_hpp
#define ruth_dev_sht31_hpp

#include "dev_i2c/i2c.hpp"

namespace i2c {

class SHT31 : public Device {
public:
  SHT31(uint8_t addr = 0x44);

  const char *description() const override { return "sht31"; }
  bool detect() override;
  bool report(const bool send) override;

private:
  bool crc(const uint8_t *data, const size_t index = 0);
};

} // namespace i2c

#endif
