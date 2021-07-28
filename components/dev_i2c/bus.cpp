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
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "bus.hpp"

namespace i2c {
static const char *TAG = "i2c:bus";

static constexpr gpio_num_t rst_pin = GPIO_NUM_21;
static constexpr uint64_t rst_sel = GPIO_SEL_21;
static constexpr gpio_num_t sda_pin = GPIO_NUM_18;
static constexpr gpio_num_t scl_pin = GPIO_NUM_19;
static constexpr TickType_t cmd_timeout = pdMS_TO_TICKS(1000);

DRAM_ATTR static TaskHandle_t bus_holder = nullptr;
DRAM_ATTR static SemaphoreHandle_t mutex = nullptr;
static i2c_config_t i2c_config = {};
static gpio_config_t rst_pin_config = {};
DRAM_ATTR static esp_err_t status = ESP_FAIL;
DRAM_ATTR static int timeout_default = 0;

IRAM_ATTR bool Bus::acquire(uint32_t timeout_ms) {
  auto rc = false;
  const UBaseType_t wait_ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

  auto bus_requestor = xTaskGetCurrentTaskHandle();

  if (bus_requestor == bus_holder) {
    return true;
  }

  auto take_rc = xSemaphoreTake(mutex, wait_ticks);

  if (take_rc == pdTRUE) {
    bus_holder = bus_requestor;
    ESP_LOGD(TAG, "ACQUIRE bus holder: %p", bus_holder);
    rc = true;
  } else {
    ESP_LOGW(TAG, "semaphore take failed: %d", take_rc);
  }

  return rc;
}

IRAM_ATTR i2c_cmd_handle_t Bus::createCmd() {
  auto cmd = i2c_cmd_link_create();

  i2c_master_start(cmd);

  return cmd;
}

IRAM_ATTR bool Bus::error() {
  auto const rc = status != ESP_OK;

  if (rc == true) {
    ESP_LOGW(TAG, "error: %d", status);
  }

  return rc;
}

IRAM_ATTR esp_err_t Bus::errorCode() { return status; }

IRAM_ATTR bool Bus::executeCmd(i2c_cmd_handle_t cmd, const float timeout_scale) {

  Bus::acquire();
  int timeout = timeout_default * timeout_scale;

  if (timeout != timeout_default) {
    i2c_set_timeout(I2C_NUM_0, timeout);
  }

  // execute queued i2c cmd
  status = i2c_master_cmd_begin(I2C_NUM_0, cmd, cmd_timeout);
  i2c_cmd_link_delete(cmd);

  if (timeout != timeout_default) {
    i2c_set_timeout(I2C_NUM_0, timeout_default);
  }

  Bus::release();

  return status == ESP_OK;
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

  mutex = xSemaphoreCreateMutex();
  xSemaphoreGive(mutex);

  ESP_LOGI(TAG, "i2c driver installed and mutex created");
  return true;
}

IRAM_ATTR bool Bus::ok() { return status == ESP_OK; }

bool Bus::release() {
  auto task = xTaskGetCurrentTaskHandle();

  if (task != bus_holder) {
    return false;
  }

  auto rc = false;

  ESP_LOGD(TAG, "RELEASE bus holder %p", bus_holder);
  bus_holder = nullptr;
  auto give_rc = xSemaphoreGive(mutex);

  if (give_rc == pdTRUE) {
    rc = true;
  } else {
    ESP_LOGW(TAG, "semaphore give failed: %d", give_rc);
  }

  return rc;
}

IRAM_ATTR bool Bus::reset() {
  auto rnd_delay = (esp_random() % 2000) + 1000;
  gpio_set_level(rst_pin, 0);           // pull the pin low to reset i2c devices
  vTaskDelay(pdMS_TO_TICKS(250));       // give plenty of time for all devices to reset
  status = gpio_set_level(rst_pin, 1);  // bring all devices online
  vTaskDelay(pdMS_TO_TICKS(rnd_delay)); // give time for devices to initialize

  if (status == ESP_OK) return true;

  ESP_LOGW(TAG, "reset failed: [%d]", status);
  return false;
}

} // namespace i2c
