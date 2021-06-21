/*
    dev_pwm/cmduence/cmduence.cpp
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
#include <cstdlib>

#include <esp_log.h>

#include "dev_pwm/cmd.hpp"

namespace pwm {

// NOTE:  all members assigned in constructor definition are constants or
//        statics and do not need to be copied

Command::Command(Hardware *hardware, JsonObject &obj) : _hw(hardware) {

  // REMINDER: we must always make a local copy of relevant info from the JsonObject
  auto name = obj["name"].as<const char *>();
  constexpr size_t name_len = sizeof(_name) / sizeof(char) - 1;
  memccpy(_name, name, 0x00, name_len);

  _task.priority = obj["pri"] | 15;
  _task.stack = obj["stack"] | 2560;

  // grab the task handle of the caller to use for later task notifications
  _parent = xTaskGetCurrentTaskHandle();
}

Command::~Command() {
  // ensure the task is stopped and deleted from the run queue
  kill();
}

void Command::fadeTo(uint32_t fade_to) {
  const auto step = 15;
  const auto delay_ms = 70;

  auto current_duty = getDuty();
  auto direction = (fade_to < current_duty) ? -1 : 1;

  uint32_t steps = labs(current_duty - fade_to) / step;

  auto new_duty = current_duty;

  for (uint32_t i = 0; (i < steps) && keepRunning(); i++) {
    new_duty += (direction * step);
    _hw->updateDuty(new_duty);

    pause(delay_ms);
  }
}

void Command::pause(uint32_t ms) {
  _notify_val = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ms));

  if (_notify_val > 0) {
    _run = false;
    // Text::rlog("\"%s\" task[%s] notify=%u", name().c_str(), taskName(), _notify_val);
  }
}

void Command::run() {
  if (_loop_func) {

    char task_name[ruth::task_max_name_len];

    auto *p = task_name;
    *p++ = 'p';
    *p++ = 'w';
    *p++ = 'm';
    *p++ = ':';

    constexpr size_t capacity = sizeof(task_name) / sizeof(char) - 5;
    memccpy(p, hardware()->shortName(), 0x00, capacity);
    xTaskCreate(&runTask, task_name, _task.stack, _task.data, _task.priority, &_task.handle);
  }
}

void Command::runTask(void *task_instance) {
  Command *cmd = (Command *)task_instance;

  cmd->_loop_func(cmd->_task.data);

  xTaskNotify(cmd->_parent, 0, eIncrement);
  cmd->kill();
}

void Command::kill() {
  // nothing to stop
  if (_task.handle == nullptr) return;

  TaskHandle_t to_delete = _task.handle;
  _task.handle = nullptr;

  // Text::rlog("\"%s\" killed, task[%s]", name().c_str(), taskName(to_delete));

  // inform FreeRTOS to remove this task from the scheduler
  vTaskDelete(to_delete); // end of task
}

} // namespace pwm
