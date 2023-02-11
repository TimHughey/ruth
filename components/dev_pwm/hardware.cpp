/*
    Ruth
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

#include "dev_pwm/hardware.hpp"

#include <cstring>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_attr.h>

namespace ruth {
namespace pwm {

static ledc_timer_config_t ledc_timer[2];
static ledc_timer_t pinToTimerMap[5] = {LEDC_TIMER_0, LEDC_TIMER_0, LEDC_TIMER_0, LEDC_TIMER_1,
                                        LEDC_TIMER_1};
static constexpr size_t num_channels = 5;
static ledc_channel_config_t channel_config[num_channels] = {};
static ledc_channel_t numToChannelMap[num_channels] = {
    LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3, LEDC_CHANNEL_4};
static gpio_num_t numToGpioMap[num_channels] = {GPIO_NUM_13, GPIO_NUM_32, GPIO_NUM_15, GPIO_NUM_33,
                                                GPIO_NUM_27};

static constexpr uint64_t pwm_gpio_pin_sel =
    (GPIO_NUM_13 | GPIO_NUM_32 | GPIO_NUM_15 | GPIO_NUM_33 | GPIO_NUM_27);

static const char *pin_name[num_channels] = {"led.0", "pin.1", "pin.2", "pin.3", "pin.4"};

// construct a new Hardware with a known address and compute the id
Hardware::Hardware(uint8_t pin_num) : _pin_num(pin_num) {
  _last_rc = allOff();
  ensureTimer();
  ensureChannel(pin_num);

  updateDuty(0);
}

esp_err_t Hardware::allOff() {
  gpio_num_t pins[] = {GPIO_NUM_13, GPIO_NUM_32, GPIO_NUM_15, GPIO_NUM_33, GPIO_NUM_27};
  static bool onetime = false;

  if (onetime) return ESP_OK;

  // ensure all pins to be used as PWM are off
  gpio_config_t pins_cfg;

  pins_cfg.pin_bit_mask = pwm_gpio_pin_sel;
  pins_cfg.mode = GPIO_MODE_OUTPUT;
  pins_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  pins_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  pins_cfg.intr_type = GPIO_INTR_DISABLE;

  auto esp_rc = gpio_config(&pins_cfg);

  constexpr size_t num_pins = sizeof(pins) / sizeof(gpio_num_t);
  for (size_t i = 0; i < num_pins; i++) {
    esp_rc = gpio_set_level(pins[i], 0);
  }

  onetime = true;

  return esp_rc;
}

IRAM_ATTR uint32_t Hardware::duty(bool *changed) {
  auto chan = numToChannelMap[_pin_num];
  auto mode = channel_config[_pin_num].speed_mode;

  uint32_t duty_now = ledc_get_duty(mode, chan);

  if (changed) {
    *changed = (duty_now == 0) || (duty_now != _duty);
  }

  _duty = duty_now;

  return _duty;
}

void Hardware::ensureChannel(uint8_t num) {
  if (_channel_configured[num]) return;
  if (_last_rc != ESP_OK) return;

  auto gpio = numToGpioMap[num];

  gpio_set_level(gpio, 0);

  auto *config = &(channel_config[num]);
  config->gpio_num = gpio;
  config->speed_mode = LEDC_HIGH_SPEED_MODE;
  config->channel = numToChannelMap[num];
  config->intr_type = LEDC_INTR_DISABLE;
  config->timer_sel = pinToTimerMap[num];
  config->duty = 0;
  config->hpoint = 0;

  _last_rc = ledc_channel_config(config);

  if (_last_rc == ESP_OK) {
    _channel_configured[num] = true;
  }
}

void Hardware::ensureTimer() {
  if (_timer_configured) return;
  if (_last_rc != ESP_OK) return;

  ledc_timer_t timers[2] = {LEDC_TIMER_0, LEDC_TIMER_1};

  for (size_t i = 0; i < 2; i++) {
    ledc_timer_t timer = timers[i];

    ledc_timer[i].speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_timer[i].duty_resolution = LEDC_TIMER_13_BIT;
    ledc_timer[i].timer_num = timer;
    ledc_timer[i].freq_hz = 5000;
    ledc_timer[i].clk_cfg = LEDC_AUTO_CLK;

    _last_rc = ledc_timer_config(&(ledc_timer[i]));

    if (_last_rc == ESP_OK) {
      _timer_configured = true;
    }
  }

  _last_rc = ledc_fade_func_install(ESP_INTR_FLAG_LEVEL1);
}

const char *Hardware::shortName() const { return pin_name[_pin_num]; }

bool Hardware::stop(uint32_t final_duty) {
  auto mode = channel_config[_pin_num].speed_mode;
  auto channel = numToChannelMap[_pin_num];
  _last_rc = ledc_stop(mode, channel, final_duty);

  if (_last_rc == ESP_OK) return true;

  return false;
}

IRAM_ATTR bool Hardware::updateDuty(uint32_t new_duty) {
  const ledc_mode_t mode = channel_config[_pin_num].speed_mode;
  const ledc_channel_t channel = numToChannelMap[_pin_num];

  if (new_duty > _duty_max) new_duty = _duty_max;

  _last_rc = ledc_set_duty_and_update(mode, channel, new_duty, 0);

  if (_last_rc == ESP_OK) return true;

  return false;
}

bool Hardware::_timer_configured = false;
bool Hardware::_channel_configured[num_channels] = {false};

} // namespace pwm
} // namespace ruth
