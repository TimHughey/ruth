/*
    i2c.hpp - Ruth I2C
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

#ifndef _ruth_i2c_engine_hpp
#define _ruth_i2c_engine_hpp

#include <cstdlib>
#include <string>

#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "devs/i2c_dev.hpp"
#include "engines/engine.hpp"

namespace ruth {

typedef struct {
  TickType_t engine;
  TickType_t convert;
  TickType_t discover;
  TickType_t report;
} i2cLastWakeTime_t;

// I2C master will check ack from slave*
#define ACK_CHECK_EN (i2c_ack_type_t)0x1
// I2C master will not check ack from slave
#define ACK_CHECK_DIS (i2c_ack_type_t)0x0

#define SDA_PIN ((gpio_num_t)18)
#define SCL_PIN ((gpio_num_t)19)

#define RST_PIN GPIO_NUM_21
#define RST_PIN_SEL GPIO_SEL_21

typedef class I2c I2c_t;
class I2c : public Engine<i2cDev_t> {

private:
  I2c();

public:
  // returns true if the instance (singleton) has been created
  static bool engineEnabled();
  static void startIfEnabled() {
    if (Profile::engineEnabled(ENGINE_I2C)) {
      _instance_()->start();
    }
  }

  static bool queuePayload(MsgPayload_t_ptr payload_ptr) {
    if (engineEnabled()) {
      // move the payload_ptr to the next function
      return _instance_()->_queuePayload(move(payload_ptr));
    } else {
      return false;
    }
  }

  //
  // Tasks
  //
  void command(void *data);
  void core(void *data);
  void discover(void *data);
  void report(void *data);

  void stop();

private:
  i2c_config_t _conf;
  const TickType_t _loop_frequency =
      Profile::engineTaskIntervalTicks(ENGINE_I2C, TASK_CORE);

  const TickType_t _discover_frequency =
      Profile::engineTaskIntervalTicks(ENGINE_I2C, TASK_DISCOVER);

  const TickType_t _report_frequency =
      Profile::engineTaskIntervalTicks(ENGINE_I2C, TASK_REPORT);

  static const uint32_t _max_buses = 8;
  bool _use_multiplexer = false;
  i2cLastWakeTime_t _last_wake;

  uint32_t _bus_selects = 0;
  uint32_t _bus_select_errors = 0;
  const TickType_t _cmd_timeout = pdMS_TO_TICKS(1000);

  DeviceAddress_t _mplex_addr = DeviceAddress(0x70);
  i2cDev_t _multiplexer_dev = i2cDev(_mplex_addr);
  int _reset_pin_level = 0;

private:
  // array is zero terminated
  DeviceAddress_t _search_addrs[12] = {
      {DeviceAddress(0x44)}, {DeviceAddress(0x5C)}, {DeviceAddress(0x20)},
      {DeviceAddress(0x21)}, {DeviceAddress(0x22)}, {DeviceAddress(0x23)},
      {DeviceAddress(0x24)}, {DeviceAddress(0x25)}, {DeviceAddress(0x26)},
      {DeviceAddress(0x27)}, {DeviceAddress(0x36)}, {DeviceAddress(0x00)}};

  static I2c_t *_instance_();
  bool commandExecute(i2cDev_t *dev, uint32_t cmd_mask, uint32_t cmd_state,
                      bool ack, const RefID_t &refid,
                      elapsedMicros &cmd_elapsed);

  DeviceAddress_t *search_addrs() { return _search_addrs; };
  inline uint32_t search_addrs_count() {
    return sizeof(_search_addrs) / sizeof(DeviceAddress_t);
  };

  // generic read device that will call the specific methods
  bool readDevice(i2cDev_t *dev);

  // specific methods to read devices
  bool readMCP23008(i2cDev_t *dev);
  bool setMCP23008(i2cDev_t *dev, uint32_t cmd_mask, uint32_t cmd_state);

  bool readSeesawSoil(i2cDev_t *dev);
  bool readSHT31(i2cDev_t *dev);

  // request data by sending command bytes and then reading the result
  // NOTE:  send and recv are executed as a single i2c transaction
  esp_err_t requestData(i2cDev_t *dev, uint8_t *send, uint8_t send_len,
                        uint8_t *recv, uint8_t recv_len,
                        esp_err_t prev_esp_rc = ESP_OK, int timeout = 0);

  // utility methods
  esp_err_t busRead(i2cDev_t *dev, uint8_t *buff, uint32_t len,
                    esp_err_t prev_esp_rc = ESP_OK);
  esp_err_t busWrite(i2cDev_t *dev, uint8_t *buff, uint32_t len,
                     esp_err_t prev_esp_rc = ESP_OK);
  bool crcSHT31(const uint8_t *data);
  bool detectDevice(i2cDev_t *dev);
  bool detectDevicesOnBus(int bus);

  bool detectMultiplexer(const int max_attempts = 1);
  bool pinReset();
  bool hardReset();
  bool installDriver();
  uint32_t maxBuses();
  bool useMultiplexer();
  bool selectBus(uint32_t bus);
  void printUnhandledDev(i2cDev_t *dev);
};
} // namespace ruth

#endif // ruth_i2c_hpp
