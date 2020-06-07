/*
    devs/pwm/cmds/basic.cpp
    Ruth PWM Basic Command Class Implementation

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

#include "devs/pwm/cmds/basic.hpp"
#include "readings/simple_text.hpp"

namespace ruth {
namespace pwm {
Basic::Basic(const char *pin, ledc_channel_config_t *chan, JsonObject &cmd)
    : Command(pin, chan, cmd) {

  JsonObject basic_obj = cmd["basic"];

  if (basic_obj) {
    // should this command repeat forever?  default to false
    _repeat = basic_obj["repeat"] | false;

    JsonArray steps_obj = basic_obj["steps"];

    for (JsonObject step_obj : steps_obj) {
      Step_t *step = new Step_t(step_obj);

      _steps.push_back(step);
    }
  }

  loopData(this);
  useLoopFunction(&loop);
}

Basic::~Basic() {
  kill(); // kill our process, if we're running

  // free the steps
  for_each(_steps.begin(), _steps.end(), [this](Step_t *step) { delete step; });
}

void Basic::_loop() {

  ST::rlog("pwm cmd \"%s\" starting", name_cstr());

  do {
    for_each(_steps.begin(), _steps.end(), [this](Step_t *step) {
      const ledc_channel_config_t *chan = channel();

      const ledc_mode_t mode = chan->speed_mode;
      const ledc_channel_t channel = chan->channel;

      auto esp_rc = ledc_set_duty_and_update(mode, channel, step->duty(), 0);

      if (esp_rc != ESP_OK) {
        ST::rlog("basic cmd ledc_set_duty failed: %s", esp_err_to_name(esp_rc));
      }

      auto notify_val = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(step->ms()));

      if (notify_val > 0) {
        ST::rlog("basic cmd notify val=%d", notify_val);
      }
    });
  } while (_repeat == true);
}
} // namespace pwm
} // namespace ruth
