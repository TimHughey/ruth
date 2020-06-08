/*
    devs/pwm/cmduence/cmduence.cpp
    Ruth PWM Command Class Implementation

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

#include "devs/pwm/cmds/cmd.hpp"
#include "readings/text.hpp"

namespace ruth {
namespace pwm {

// NOTE:  all members assigned in constructor definition are constants or
//        statics and do not need to be copied

Command::Command(const char *pin, ledc_channel_config_t *chan, JsonObject &obj)
    : _pin(pin), _channel(chan) {

  // grab a const char * to the name so we can make a local copy.
  // REMINDER we must always make a local copy of data from the JsonDocument
  const char *name_str = obj["name"];
  _name.assign(name_str);

  // should this command immediately go active? default to true if not specified
  _activate = obj["activate"] | true;
  // grab the task handle of the caller to use for later task notifications
  _parent = xTaskGetCurrentTaskHandle();
}

Command::~Command() {
  // ensure the task is stopped and deleted from the run queue
  kill();
}

void Command::pause(uint32_t ms) {
  _notify_val = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ms));

  if (_notify_val > 0) {
    _run = false;
    Text::rlog("cmd \"%s\" on \"%s\" task notify=%ld", name_cstr(), pin(),
             _notify_val);
  }
}

void Command::runIfNeeded() {
  if (_activate)
    _start_();
}

void Command::kill() {
  // nothing to stop
  if (_task.handle == nullptr)
    return;

  TaskHandle_t to_delete = _task.handle;
  _task.handle = nullptr;

  auto stack_hw = uxTaskGetStackHighWaterMark(nullptr);

  Text::rlog("killing cmd \"%s\" notify=%ld handle=%p stack_hw=%d", name_cstr(),
           _notify_val, to_delete, stack_hw);

  // inform FreeRTOS to remove this task from the scheduler
  vTaskDelete(to_delete);

  // NOTE: nothing can be returned from this function as code after
  //       vTaskDelete is never executed
}

} // namespace pwm
} // namespace ruth
