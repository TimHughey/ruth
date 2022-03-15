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

#include <cstring>

#include <driver/i2c.h>
#include <esp_attr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "bus.hpp"

namespace i2c {

static constexpr gpio_num_t rst_pin = GPIO_NUM_21;
static constexpr uint64_t rst_sel = GPIO_SEL_21;
static constexpr gpio_num_t sda_pin = GPIO_NUM_23;
static constexpr gpio_num_t scl_pin = GPIO_NUM_22;
static constexpr TickType_t cmd_timeout = pdMS_TO_TICKS(100);

DRAM_ATTR SemaphoreHandle_t Bus::mutex = nullptr;
DRAM_ATTR static i2c_config_t i2c_config = {};
DRAM_ATTR static gpio_config_t rst_pin_config = {};

DRAM_ATTR esp_err_t Bus::status = ESP_FAIL;
DRAM_ATTR static int timeout_default = 0;

DRAM_ATTR uint8_t Bus::txn_buff[Bus::_size];

IRAM_ATTR bool Bus::executeCmd(i2c_cmd_handle_t cmd, const float timeout_scale) {
  Bus::status = ESP_FAIL;

  if (Bus::acquire(10000) == true) {
    const int timeout = timeout_default * timeout_scale;

    if (timeout != timeout_default) i2c_set_timeout(I2C_NUM_0, timeout);

    // execute queued i2c cmd
    Bus::status = i2c_master_cmd_begin(I2C_NUM_0, cmd, cmd_timeout);
    i2c_cmd_link_delete_static(cmd);

    if (timeout != timeout_default) i2c_set_timeout(I2C_NUM_0, timeout_default);

    Bus::release();
  }

  return Bus::status == ESP_OK;
}

bool Bus::init() {
  rst_pin_config.pin_bit_mask = rst_sel;
  rst_pin_config.mode = GPIO_MODE_OUTPUT;
  rst_pin_config.pull_up_en = GPIO_PULLUP_DISABLE;
  rst_pin_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  rst_pin_config.intr_type = GPIO_INTR_DISABLE;

  if (gpio_config(&rst_pin_config) != ESP_OK) return false;

  i2c_config.mode = I2C_MODE_MASTER;
  i2c_config.sda_io_num = sda_pin;
  i2c_config.scl_io_num = scl_pin;
  i2c_config.sda_pullup_en = GPIO_PULLUP_ENABLE;
  i2c_config.scl_pullup_en = GPIO_PULLUP_ENABLE;
  i2c_config.master.clk_speed = 100000;

  if (i2c_param_config(I2C_NUM_0, &i2c_config) != ESP_OK) return false;
  if (i2c_driver_install(I2C_NUM_0, i2c_config.mode, 0, 0, 0) != ESP_OK) return false;

  i2c_get_timeout(I2C_NUM_0, &timeout_default);
  i2c_filter_enable(I2C_NUM_0, 1);

  // simply pull up the reset pin
  // previous reset logic no longer required; esp-idf has bus clear logic

  constexpr auto power_on_ticks = pdMS_TO_TICKS(500);
  Bus::status = gpio_set_level(rst_pin, 1); // bring all devices online
  vTaskDelay(power_on_ticks);               // give time for devices to initialize

  if (Bus::status != ESP_OK) return false;

  mutex = xSemaphoreCreateMutex();
  if (mutex == nullptr) return false;

  return release();
}

} // namespace i2c
