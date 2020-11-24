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

#ifndef _ruth_ds_engine_hpp
#define _ruth_ds_engine_hpp

#include <cstdlib>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "devs/ds/dev.hpp"
#include "drivers/owb.h"
#include "drivers/owb_rmt.h"
#include "engines/engine.hpp"

namespace ruth {

typedef class DallasSemi DallasSemi_t;
class DallasSemi : public Engine<DsDevice_t, ENGINE_DALSEMI> {

private:
  DallasSemi();

public:
  static bool engineEnabled();
  static void startIfEnabled() {
    if (Profile::engineEnabled(ENGINE_DALSEMI)) {
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

  void report(void *data);

  void stop();

protected:
  bool resetBus();
  bool resetBus(bool &present);

private:
  uint8_t _pin = 14;
  OneWireBus *_ds = nullptr;

  bool _devices_powered = true;
  bool _temp_devices_present = false;

  const TickType_t _temp_convert_wait_ms = 10;

  const uint32_t _max_temp_convert_ms = 1000; // one second

  static DallasSemi_t *_instance_();

  bool checkDevicesPowered();
  bool commandExecute(DsDevice_t *dev, uint32_t cmd_mask, uint32_t cmd_state,
                      bool ack, const RefID_t &refid,
                      elapsedMicros &cmd_elapsed);

  bool discover();

  bool devicesPowered() { return _devices_powered; }
  void haveTemperatureDevices() { _temp_devices_present = true; }

  bool readDevice(DsDevice_t *dev);

  // specific methods to read devices
  bool readDS1820(DsDevice_t *dev, Sensor_t **reading);
  bool readDS2408(DsDevice_t *dev, Positions_t **reading = nullptr);
  bool readDS2406(DsDevice_t *dev, Positions_t **reading);
  bool readDS2413(DsDevice_t *dev, Positions_t **reading);

  bool setDS2406(DsDevice_t *dev, uint32_t cmd_mask, uint32_t cmd_state);
  bool setDS2408(DsDevice_t *dev, uint32_t cmd_mask, uint32_t cmd_state);
  bool setDS2413(DsDevice_t *dev, uint32_t cmd_mask, uint32_t cmd_state);

  bool temperatureConvert();
  bool tempDevicesPresent() { return _temp_devices_present; }

  static bool check_crc16(const uint8_t *input, uint16_t len,
                          const uint8_t *inverted_crc, uint16_t crc = 0);
  static uint16_t crc16(const uint8_t *input, uint16_t len, uint16_t crc);
};
} // namespace ruth

#endif // ruth_ds_engine_hpp
