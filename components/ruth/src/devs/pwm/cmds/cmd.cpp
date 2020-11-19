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
  _name = obj["name"].as<const char *>();

  // grab the task handle of the caller to use for later task notifications
  _parent = xTaskGetCurrentTaskHandle();
}

Command::~Command() {
  // ensure the task is stopped and deleted from the run queue
  kill();
}

void Command::fadeTo(uint32_t fade_to) {
  const auto step = 7;
  const auto delay_ms = 55;

  auto current_duty = getDuty();
  auto direction = (fade_to < current_duty) ? -1 : 1;

  uint32_t steps = labs(current_duty - fade_to) / step;

  auto new_duty = current_duty;

  for (auto i = 0; (i < steps) && keepRunning(); i++) {
    new_duty += (direction * step);
    setDuty(new_duty);

    pause(delay_ms);
  }
}

uint32_t Command::getDuty() {
  const ledc_channel_config_t *chan = channel();
  const ledc_mode_t mode = chan->speed_mode;
  const ledc_channel_t channel = chan->channel;

  auto duty = ledc_get_duty(mode, channel);

  return duty;
}

void Command::pause(uint32_t ms) {
  _notify_val = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ms));

  if (_notify_val > 0) {
    _run = false;
    Text::rlog("cmd \"%s\" on \"%s\" task notify=%ld", name().c_str(), pin(),
               _notify_val);
  }
}

bool Command::run() {
  _start_();
  return true;
}

esp_err_t Command::setDuty(uint32_t duty) {
  const ledc_channel_config_t *chan = channel();
  const ledc_mode_t mode = chan->speed_mode;
  const ledc_channel_t channel = chan->channel;

  return ledc_set_duty_and_update(mode, channel, duty, 0);
}

void Command::kill() {
  // nothing to stop
  if (_task.handle == nullptr)
    return;

  TaskHandle_t to_delete = _task.handle;
  _task.handle = nullptr;

  auto stack_hw = uxTaskGetStackHighWaterMark(to_delete);

  Text::rlog("killing cmd \"%s\" notify=%ld handle=%p stack_hw=%d",
             name().c_str(), _notify_val, to_delete, stack_hw);

  // inform FreeRTOS to remove this task from the scheduler
  vTaskDelete(to_delete);

  // NOTE: nothing can be returned from this function as code after
  //       vTaskDelete is never executed
}

} // namespace pwm
} // namespace ruth
