/*
    misc/status_led.cpp - Ruth Status LED Support
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

#include "status_led.hpp"

namespace ruth {
// static const char *TAG = "StatusLED";
static StatusLED_t __singleton__;

void StatusLED::_init() {
  auto timer_rc = ESP_FAIL;
  auto config_rc = ESP_FAIL;
  auto fade_func_rc = ESP_FAIL;

  timer_rc = ledc_timer_config(&ledc_timer_);

  if (timer_rc == ESP_OK) {
    config_rc = ledc_channel_config(&ledc_channel_);

    if (config_rc == ESP_OK) {
      fade_func_rc = ledc_fade_func_install(ESP_INTR_FLAG_LEVEL1);

      if (fade_func_rc == ESP_OK) {
        ledc_set_duty_and_update(ledc_channel_.speed_mode, ledc_channel_.channel, duty_, 0);
      }
    }
  }
}

void StatusLED::_bright_() {
  duty_ = duty_max_ / 2;
  activate_duty();
}

void StatusLED::_brighter_() {
  duty_ += 1024;

  if (duty_ > duty_max_) {
    duty_ = duty_max_;
  }

  activate_duty();
}

void StatusLED::_dim_() {
  duty_ = duty_max_ / 90;
  activate_duty();
}

void StatusLED::_dimmer_() {
  if (duty_ > 1025) {
    duty_ -= 1025;
  }

  activate_duty();
}

void StatusLED::_off_() {
  duty_ = 0;
  activate_duty();
}

void StatusLED::_percent_(float p) {
  uint32_t val = uint32_t(duty_max_ * p);

  duty_ = val;
  activate_duty();
}

void StatusLED::activate_duty() {
  ledc_set_duty_and_update(ledc_channel_.speed_mode, ledc_channel_.channel, duty_, 0);
}

void StatusLED::_duty_(uint32_t new_duty) {

  if (new_duty <= duty_max_) {
    duty_ = new_duty;

    activate_duty();
  }
}

// STATIC
StatusLED_t *StatusLED::_instance_() { return &__singleton__; }
} // namespace ruth
