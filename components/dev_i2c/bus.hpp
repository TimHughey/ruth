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

#ifndef ruth_i2c_bus_hpp
#define ruth_i2c_bus_hpp

#include <memory>

#include <driver/i2c.h>

namespace i2c {

class Bus {
public:
  static bool acquire(const uint32_t timeout_ms);
  static i2c_cmd_handle_t createCmd();
  static bool executeCmd(i2c_cmd_handle_t cmd, const float timeout_scale = 1.0);

  static bool error();
  static esp_err_t errorCode() { return status; }
  static void delay(uint32_t ms);
  static bool init();

  static bool release();

private:
  static esp_err_t status;
};
} // namespace i2c

#endif
