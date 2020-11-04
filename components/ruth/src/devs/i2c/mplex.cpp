/*
    mplex.cpp - Ruth I2C Multiplexer Device
    Copyright (C) 2017  Tim Hughey

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

#include <memory>
#include <string>

#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <sys/time.h>
#include <time.h>

#include "devs/base/base.hpp"
#include "devs/i2c/mplex.hpp"
#include "local/types.hpp"
#include "net/network.hpp"

namespace ruth {
TCA9548B::TCA9548B() : I2cDevice(DeviceAddress(0x70)) {}

bool TCA9548B::detect() {
  // when detecting a device always reset the previous command esp_err_t
  clearPreviousError();

  // to the detect the multiplexer we:
  //  1.  write 0x00 to the control register (disable all buses)
  //  2.  read the control register
  //  3.  confirm the control register is 0x00 (all buses disabled)
  _tx = {0x00};
  // expect a single byte response
  _rx = {0x00};

  // if the device read succeeded and the control register is 0x00
  // the multiplexer is available
  if ((requestData() == ESP_OK) && (_rx[0] == 0x00)) {
    return true;
  }

  return false;
}

} // namespace ruth
