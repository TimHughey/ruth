/*
    pwm_engine.hpp - Ruth PWM Engine
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

#ifndef ruth_pwm_engine_hpp
#define ruth_pwm_engine_hpp

#include <cstdlib>
#include <string>

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "devs/pwm_dev.hpp"
#include "engines/engine.hpp"
#include "protocols/payload.hpp"

namespace ruth {

using std::unordered_map;

typedef struct {
  TickType_t engine;
  TickType_t convert;
  TickType_t discover;
  TickType_t report;
} pwmLastWakeTime_t;

typedef class PulseWidth PulseWidth_t;
class PulseWidth : public Engine<pwmDev_t> {

private:
  PulseWidth();

public:
  // returns true if the instance (singleton) has been created
  static bool engineEnabled();
  static void startIfEnabled() {
    if (Profile::pwmEnable()) {
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
  void core(void *data);
  void discover(void *data);
  void report(void *data);

  void stop();

private:
  const TickType_t _loop_frequency = pdMS_TO_TICKS(10000);
  const TickType_t _discover_frequency = pdMS_TO_TICKS(59000);
  const TickType_t _report_frequency = pdMS_TO_TICKS(10000);

  pwmLastWakeTime_t _last_wake;

private:
  static PulseWidth_t *_instance_();

  bool commandExecute(JsonDocument &doc);

  // generic read device that will call the specific methods
  bool readDevice(pwmDev_t *dev);

  bool configureTimer();
  bool detectDevice(pwmDev_t *dev);
};
} // namespace ruth

#endif // pwm_engine_hpp
