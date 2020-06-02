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

Sequence::Sequence(const char *pin_desc, const char *name, xTaskHandle parent,
                   ledc_channel_config_t *channel, JsonObject &obj)
    : _pin_desc(pin_desc), _parent(parent), _channel(channel) {

  _name.assign(name); // copy the name from the JsonObject
  _repeat = obj["repeat"] | false;
  _active = obj["activate"] | true;

  JsonArray steps_obj = obj["steps"];

  for (JsonObject step_obj : steps_obj) {
    Step_t *step = new Step_t(step_obj);

    _steps.push_back(step);
  }
}

Sequence::~Sequence() {
  stop();

  // free the steps
  for_each(_steps.begin(), _steps.end(), [this](Step_t *step) { delete step; });
}

void Sequence::loop(void *data) {

  ST::rlog("sequence \"%s\" starting", _name.c_str());

  do {
    for_each(_steps.begin(), _steps.end(), [this](Step_t *step) {
      const ledc_mode_t mode = _channel->speed_mode;
      const ledc_channel_t channel = _channel->channel;

      auto esp_rc = ledc_set_duty_and_update(mode, channel, step->duty(), 0);

      if (esp_rc != ESP_OK) {
        ST::rlog("sequence ledc_set_duty failed: %s", esp_err_to_name(esp_rc));
      }

      auto notify_val = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(step->ms()));

      if (notify_val > 0) {
        ST::rlog("sequence notify val=%d", notify_val);
      }
    });
  } while (_repeat == true);

  ST::rlog("sequence \"%s\" finished", _name.c_str());

  xTaskNotify(_parent, 0, eIncrement);
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
