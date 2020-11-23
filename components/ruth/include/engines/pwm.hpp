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

#ifndef _ruth_pwm_engine_hpp
#define _ruth_pwm_engine_hpp

#include <cstdlib>

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "devs/pwm/pwm.hpp"
#include "engines/engine.hpp"
#include "protocols/payload.hpp"

namespace ruth {
using std::move;
using namespace reading;

typedef class PulseWidth PulseWidth_t;
class PulseWidth : public Engine<PwmDevice_t> {

private:
  PulseWidth();

public:
  // returns true if the instance (singleton) has been created
  static bool engineEnabled();
  static void startIfEnabled() {
    if (Profile::engineEnabled(ENGINE_PWM)) {
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

  void core(void *data);
  void report(void *data);

  void commandLocal(MsgPayload_t_ptr payload);

  void stop();

private:
  const TickType_t _loop_frequency =
      Profile::engineTaskIntervalTicks(ENGINE_PWM, TASK_CORE);

  const TickType_t _report_frequency =
      Profile::engineTaskIntervalTicks(ENGINE_PWM, TASK_REPORT);

  EngineTask_t _tasks[3];

private:
  static PulseWidth_t *_instance_();

  bool commandExecute(JsonDocument &doc);

  // generic read device that will call the specific methods
  bool readDevice(PwmDevice_t *dev);

  void configureTimer();
  bool detectDevice(PwmDevice_t *dev);
};
} // namespace ruth

#endif // pwm_engine_hpp
