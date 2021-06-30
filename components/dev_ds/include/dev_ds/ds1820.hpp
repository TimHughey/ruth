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

#ifndef ruth_dev_ds1820_hpp
#define ruth_dev_ds1820_hpp

#include <memory>

#include "ArduinoJson.h"
#include "dev_ds/ds.hpp"

namespace ds {
class DS1820 : public Device {

public:
  DS1820(const uint8_t *addr);

  bool report() override;

private:
  bool celsius(float &val);
};
} // namespace ds

#endif
