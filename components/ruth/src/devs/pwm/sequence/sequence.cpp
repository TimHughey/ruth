/*
    devs/pwm/sequence/sequence.cpp
    Ruth PWM Sequence Class Implementation

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

#include <algorithm>

#include <esp_log.h>

#include "devs/pwm/sequence/sequence.hpp"
#include "readings/simple_text.hpp"

namespace ruth {
namespace pwm {

// NOTE:  all members assigned in constructor definition are constants or
//        statics and do not need to be copied

Sequence::Sequence(const char *pin, ledc_channel_config_t *chan,
                   JsonObject &obj)
    : _pin(pin), _channel(chan) {

  // grab a const char * to the name so we can make a local copy.
  // REMINDER we must always make a local copy of data from the JsonDocument
  const char *name_str = obj["name"];
  _name.assign(name_str);

  // grab the activate flag
  _active = obj["activate"] | true;

  // grab the task handle of the caller to use for later task notifications
  _parent = xTaskGetCurrentTaskHandle();
}

Sequence::~Sequence() {
  // ensure the taks is stopped and deleted from the run queue, nothing
  // else to deallocate

  stop();
}

void Sequence::stop() {
  // nothing to stop
  if (_task.handle == nullptr)
    return;

  TaskHandle_t to_delete = _task.handle;
  _task.handle = nullptr;

  auto stack_hw = uxTaskGetStackHighWaterMark(nullptr);

  ST::rlog("sequence deleting task=%p stack_hw=%d", to_delete, stack_hw);

  // inform FreeRTOS to remove this task from the scheduler
  vTaskDelete(to_delete);
}

} // namespace pwm
} // namespace ruth
