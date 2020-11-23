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

#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "devs/i2c/base.hpp"
#include "devs/i2c/mcp23008.hpp"
#include "devs/i2c/mplex.hpp"
#include "devs/i2c/sht31.hpp"
#include "engines/engine.hpp"

namespace ruth {

#define SDA_PIN ((gpio_num_t)18)
#define SCL_PIN ((gpio_num_t)19)

#define RST_PIN GPIO_NUM_21
#define RST_PIN_SEL GPIO_SEL_21

typedef class I2c I2c_t;
class I2c : public Engine<I2cDevice_t, ENGINE_I2C> {

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

  // engine.hpp virtual implementation
  bool readDevice(I2cDevice_t *dev) { return dev->read(); }

private:
  i2c_config_t _conf;
  const TickType_t _loop_frequency =
      Profile::engineTaskIntervalTicks(ENGINE_I2C, TASK_CORE);

  const TickType_t _discover_frequency =
      Profile::engineTaskIntervalTicks(ENGINE_I2C, TASK_DISCOVER);

  const TickType_t _report_frequency =
      Profile::engineTaskIntervalTicks(ENGINE_I2C, TASK_REPORT);

  const time_t _dev_missing_secs =
      ((_report_frequency * 1.5) / portTICK_PERIOD_MS) * 1000 * 60;

  bool _use_multiplexer = false;

  const TickType_t _cmd_timeout = pdMS_TO_TICKS(1000);

  I2cMultiplexer_t _mplex;
  int _reset_pin_level = 0;

private:
  static I2c_t *_instance_();
  bool commandExecute(I2cDevice_t *dev, uint32_t cmd_mask, uint32_t cmd_state,
                      bool ack, const RefID_t &refid,
                      elapsedMicros &cmd_elapsed);

  // utility methods
  // esp_err_t busRead(I2cDevice_t *dev, uint8_t *buff, uint32_t len,
  //                   esp_err_t prev_esp_rc = ESP_OK);

  bool detectDevicesOnBus(int bus);

  bool detectMultiplexer();
  bool pinReset();
  bool hardReset();
  bool installDriver();
  bool useMultiplexer() const { return _use_multiplexer; }
  bool selectBus(uint32_t bus);
  void printUnhandledDev(I2cDevice_t *dev);
};
} // namespace ruth

#endif // ruth_i2c_hpp
