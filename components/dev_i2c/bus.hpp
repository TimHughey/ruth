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
  inline static bool acquire(const uint32_t timeout_ms) {
    const UBaseType_t wait_ticks = pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(mutex, wait_ticks) == pdTRUE;
  }

  inline static i2c_cmd_handle_t createCmd() {
    auto cmd = i2c_cmd_link_create_static(Bus::txn_buff, Bus::_size);

    i2c_master_start(cmd);

    return cmd;
  }

  static bool executeCmd(i2c_cmd_handle_t cmd, const float timeout_scale = 1.0);

  inline static bool error() { return status != ESP_OK; }
  inline static esp_err_t errorCode() { return status; }
  static bool init();
  inline static bool release() { return xSemaphoreGive(mutex) == pdTRUE; }

private:
  static SemaphoreHandle_t mutex;
  static esp_err_t status;

  static constexpr auto txn_max = 7;
  static constexpr uint32_t _size = I2C_LINK_RECOMMENDED_SIZE(txn_max);
  static uint8_t txn_buff[_size];
};
} // namespace i2c

#endif
