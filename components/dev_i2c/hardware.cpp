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
#include <ctime>
#include <sys/time.h>

#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_attr.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "bus.hpp"
#include "dev_i2c/hardware.hpp"

namespace i2c {

static const char *TAG = "ds:hardware";

Hardware::Hardware(const uint8_t addr) : _addr(addr) { makeID(); }

IRAM_ATTR bool Hardware::requestData(const DataRequest &request) {
  i2c_cmd_handle_t cmd = nullptr;
  int timeout = timeout_default * request.timeout_scale;

  // if (_esp_rc_prev != ESP_OK) {
  //   return false;
  // }

  // readStart();

  if (timeout != timeout_default) {
    i2c_set_timeout(I2C_NUM_0, timeout);
  }

  cmd = i2c_cmd_link_create(); // allocate i2c cmd queue
  i2c_master_start(cmd);       // queue i2c START

  // if there are bytes to TX queue them
  if (request.tx.len > 0) {
    // queue the WRITE for device and check for ACK
    i2c_master_write_byte(cmd, (request.addr << 1) | I2C_MASTER_WRITE, true);

    // queue the device command bytes
    i2c_master_write(cmd, request.tx.data, request.tx.len, I2C_MASTER_ACK);
  }

  // clock stretching is leveraged in the event the device requires time
  // to execute the command (e.g. temperature conversion)
  // use timeout scale to adjust time to wait for clock, if needed
  if (request.rx.len > 0) {
    // start a new command sequence without sending a stop
    i2c_master_start(cmd);

    // queue the READ for device and check for ACK
    i2c_master_write_byte(cmd, (request.addr << 1) | I2C_MASTER_READ, true);

    // queue the READ of number of bytes
    i2c_master_read(cmd, request.rx.data, request.rx.len, I2C_MASTER_LAST_NACK);
  }

  // always queue the stop command
  i2c_master_stop(cmd);

  // execute queued i2c cmd
  status = i2c_master_cmd_begin(I2C_NUM_0, cmd, cmd_timeout);
  i2c_cmd_link_delete(cmd);

  // if the timeout was changed restore it
  if (timeout != timeout_default) {
    i2c_set_timeout(I2C_NUM_0, timeout_default);
  }

  // _esp_rc_prev = _esp_rc;
  //
  // readStop();
  return ok();
}

} // namespace i2c
