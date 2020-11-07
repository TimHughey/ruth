/*
    i2c/seesaw.cpp - Ruth I2C Device Seesaw
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

#include <driver/i2c.h>
#include <esp_log.h>

#include "devs/i2c/seesaw.hpp"

namespace ruth {

Seesaw::Seesaw(uint8_t bus, uint8_t addr) : I2cDevice(addr, bus) {}

Seesaw::Seesaw(const Seesaw_t &rhs, time_t missing_secs)
    : I2cDevice(rhs.address(), rhs.bus(), missing_secs) {
  makeID();
}

bool Seesaw::read() {
  auto rc = false;
  esp_err_t esp_rc;
  float tempC = 0.0;
  int soil_moisture;

  // seesaw data queries are two bytes that describe:
  //   1. module
  //   2. register
  // NOTE: array is ONLY for the transmit since the response varies
  //       by module and register
  _tx = {0x00,  // MSB: module
         0x00}; // LSB: register

  // seesaw responses to data queries vary in length.  this buffer will be
  // reused for all queries so it must be the max of all response lengths
  //   1. capacitance - 16bit integer (two bytes)
  //   2. temperature - 32bit float (four bytes)
  _rx = {
      0x00, 0x00, // capactive: (int)16bit, temperature: (float)32 bits
      0x00, 0x00  // capactive: not used, temperature: (float)32 bits
  };

  // address i2c device
  // write request to read module and register
  //  temperture: SEESAW_STATUS_BASE, SEEWSAW_STATUS_TEMP  (4 bytes)
  //     consider other status: SEESAW_STATUS_HW_ID, *VERSION, *OPTIONS
  //  capacitance:  SEESAW_TOUCH_BASE, SEESAW_TOUCH_CHANNEL_OFFSET (2 bytes)
  // delay (maybe?)
  // write request to read bytes of the register

  readStart();

  // first, request and receive the onboard temperature
  _tx = {0x00,  // SEESAW_STATUS_BASE
         0x04}; // SEESAW_STATUS_TEMP
  esp_rc = busWrite(_tx);
  delay(20);
  esp_rc = busRead(dev, buff, 4, esp_rc);

  // conversion copied from AdaFruit Seesaw library
  tempC = (1.0 / (1UL << 16)) *
          (float)(((uint32_t)buff[0] << 24) | ((uint32_t)buff[1] << 16) |
                  ((uint32_t)buff[2] << 8) | (uint32_t)buff[3]);

  // second, request and receive the touch capacitance (soil moisture)
  data_request[0] = 0x0f; // SEESAW_TOUCH_BASE
  data_request[1] = 0x10; // SEESAW_TOUCH_CHANNEL_OFFSET

  esp_rc = busWrite(dev, data_request, 2);
  delay(20);
  esp_rc = busRead(dev, buff, 2, esp_rc);

  soil_moisture = ((uint16_t)buff[0] << 8) | buff[1];

  // third, request and receive the board version
  // data_request[0] = 0x00; // SEESAW_STATUS_BASE
  // data_request[1] = 0x02; // SEESAW_STATUS_VERSION
  //
  // esp_rc = bus_write(dev, data_request, 2);
  // esp_rc = bus_read(dev, buff, 4, esp_rc);
  //
  // sw_version = ((uint32_t)buff[0] << 24) | ((uint32_t)buff[1] << 16) |
  //              ((uint32_t)buff[2] << 8) | (uint32_t)buff[3];

  dev->readStop();

  if (esp_rc == ESP_OK) {
    dev->justSeen();

    Reading_t *reading = new Sensor(dev->id(), tempC, soil_moisture);

    dev->setReading(reading);
    rc = true;
  } else {
    Text::rlog("[%s] device \"%s\" read issue", esp_err_to_name(esp_rc),
               dev->id().c_str());
  }

  return rc;
}
} // namespace ruth
