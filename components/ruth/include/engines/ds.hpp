/*
    DallasSemi - Ruth Dallas Semiconductor
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

#ifndef ruth_ds_engine_hpp
#define ruth_ds_engine_hpp

#include <cstdlib>
#include <map>
#include <string>

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "devs/ds_dev.hpp"
#include "drivers/owb.h"
#include "drivers/owb_rmt.h"
#include "engines/engine.hpp"

namespace ruth {

typedef class DallasSemi DallasSemi_t;
class DallasSemi : public Engine<dsDev_t> {

private:
  DallasSemi();

public:
  static bool engineEnabled();
  static void startIfEnabled() {
    if (Profile::dalsemiEnable()) {
      _instance_()->start();
    }
  }

  static bool queuePayload(MsgPayload_t *payload) {
    if (engineEnabled()) {
      return _instance_()->_queuePayload(payload);
    } else {
      return false;
    }
  }

  //
  // Tasks
  //
  void command(void *data);
  void convert(void *data);
  void core(void *data);
  void discover(void *data);
  void report(void *data);

  void stop();

protected:
  bool resetBus(bool *present = nullptr);

private:
  uint8_t _pin = 14;
  OneWireBus *_ds = nullptr;

  bool _devices_powered = true;
  bool _temp_devices_present = true;

  // delay times
  const TickType_t _loop_frequency =
      Profile::subSystemTaskInterval("ds", "core");

  const TickType_t _convert_frequency =
      Profile::subSystemTaskInterval("ds", "convert");

  const TickType_t _discover_frequency =
      Profile::subSystemTaskInterval("ds", "discover");

  const TickType_t _report_frequency =
      Profile::subSystemTaskInterval("ds", "report");

  const TickType_t _temp_convert_wait = pdMS_TO_TICKS(50);

  const uint64_t _max_temp_convert_us = (1000 * 1000); // one second

  static DallasSemi_t *_instance_();

  bool checkDevicesPowered();
  bool commandExecute(dsDev_t *dev, uint32_t cmd_mask, uint32_t cmd_state,
                      bool ack, const RefID_t &refid,
                      elapsedMicros &cmd_elapsed);

  bool devicesPowered() { return _devices_powered; }

  bool readDevice(dsDev_t *dev);

  // specific methods to read devices
  bool readDS1820(dsDev_t *dev, celsiusReading_t **reading);
  bool readDS2408(dsDev_t *dev, positionsReading_t **reading = nullptr);
  bool readDS2406(dsDev_t *dev, positionsReading_t **reading);
  bool readDS2413(dsDev_t *dev, positionsReading_t **reading);

  bool setDS2406(dsDev_t *dev, uint32_t cmd_mask, uint32_t cmd_state);
  bool setDS2408(dsDev_t *dev, uint32_t cmd_mask, uint32_t cmd_state);
  bool setDS2413(dsDev_t *dev, uint32_t cmd_mask, uint32_t cmd_state);

  // FIXME:  hard code there are always temperature devices
  bool tempDevicesPresent() { return _temp_devices_present; }

  static bool check_crc16(const uint8_t *input, uint16_t len,
                          const uint8_t *inverted_crc, uint16_t crc = 0);
  static uint16_t crc16(const uint8_t *input, uint16_t len, uint16_t crc);
};
} // namespace ruth

#endif // ruth_ds_engine_hpp
