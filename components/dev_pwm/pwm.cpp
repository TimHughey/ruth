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

#include <cstring>

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_attr.h>
#include <esp_log.h>
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>

// #include "dev_pwm/cmd_random.hpp"
#include "dev_pwm/pwm.hpp"

namespace device {

bool PulseWidth::_timer_configured = false;
static ledc_timer_config_t ledc_timer;

static constexpr size_t num_channels = 5;
bool PulseWidth::_channel_configured[num_channels] = {false};
static ledc_channel_config_t channel_config[num_channels] = {};
static ledc_channel_t numToChannelMap[num_channels] = {LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2,
                                                       LEDC_CHANNEL_3, LEDC_CHANNEL_4};
static gpio_num_t numToGpioMap[num_channels] = {GPIO_NUM_13, GPIO_NUM_32, GPIO_NUM_15, GPIO_NUM_33,
                                                GPIO_NUM_27};

static constexpr uint64_t pwm_gpio_pin_sel =
    (GPIO_SEL_13 | GPIO_SEL_32 | GPIO_SEL_15 | GPIO_SEL_33 | GPIO_SEL_27);

static const char *pin_name[num_channels] = {"led:0", "pin:1", "pin:2", "pin:3", "pin:4"};

static char base_name[32] = {};

// construct a new PulseWidth with a known address and compute the id
PulseWidth::PulseWidth(uint8_t num) : _num(num) {
  _last_rc = allOff();
  ensureTimer();
  ensureChannel(num);

  makeID();
}

esp_err_t PulseWidth::allOff() {
  gpio_num_t pins[] = {GPIO_NUM_13, GPIO_NUM_32, GPIO_NUM_15, GPIO_NUM_33, GPIO_NUM_27};

  // ensure all pins to be used as PWM are off
  gpio_config_t pins_cfg;

  pins_cfg.pin_bit_mask = pwm_gpio_pin_sel;
  pins_cfg.mode = GPIO_MODE_OUTPUT;
  pins_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  pins_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  pins_cfg.intr_type = GPIO_INTR_DISABLE;

  auto esp_rc = gpio_config(&pins_cfg);

  constexpr size_t num_pins = sizeof(pins) / sizeof(gpio_num_t);
  for (int i = 0; i < num_pins; i++) {
    esp_rc = gpio_set_level(pins[i], 0);
  }

  return esp_rc;
}

const std::unique_ptr<char[]> PulseWidth::debug() {
  const auto max_len = 127;
  std::unique_ptr<char[]> debug_str(new char[max_len + 1]);
  auto duty_now = duty();
  auto channel = numToChannelMap[_num];
  auto gpio = numToGpioMap[_num];

  snprintf(debug_str.get(), max_len, "PulseWidth(%s duty=%d channel=%d pin=%d last_rc=%s)", id(), duty_now,
           channel, gpio, esp_err_to_name(_last_rc));

  return std::move(debug_str);
}

const char *PulseWidth::description() const { return pin_name[_num]; }

IRAM_ATTR uint32_t PulseWidth::duty() const {
  auto chan = numToChannelMap[_num];
  auto mode = channel_config[_num].speed_mode;

  return ledc_get_duty(mode, chan);
}

void PulseWidth::ensureChannel(uint8_t num) {
  if (_channel_configured[num]) return;
  if (_last_rc != ESP_OK) return;

  auto gpio = numToGpioMap[num];

  gpio_set_level(gpio, 0);

  auto *config = &(channel_config[num]);
  config->gpio_num = gpio;
  config->speed_mode = LEDC_HIGH_SPEED_MODE;
  config->channel = numToChannelMap[num];
  config->intr_type = LEDC_INTR_DISABLE;
  config->timer_sel = LEDC_TIMER_0;
  config->duty = 0;
  config->hpoint = 0;

  _last_rc = ledc_channel_config(config);

  if (_last_rc == ESP_OK) {
    _channel_configured[num] = true;
  }
}

void PulseWidth::ensureTimer() {
  if (_timer_configured) return;
  if (_last_rc != ESP_OK) return;

  ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_timer.duty_resolution = LEDC_TIMER_13_BIT;
  ledc_timer.timer_num = LEDC_TIMER_0;
  ledc_timer.freq_hz = 5000;
  ledc_timer.clk_cfg = LEDC_AUTO_CLK;

  _last_rc = ledc_timer_config(&ledc_timer);

  if (_last_rc == ESP_OK) {
    _timer_configured = true;
  }

  _last_rc = ledc_fade_func_install(ESP_INTR_FLAG_LEVEL1);
}

// bool PulseWidth::cmd(uint32_t pwm_cmd, JsonDocument &doc) {
//   auto rc = false;
//
//   // this is a new cmd so always kill the running command, if one exists
//   cmdKill();
//
//   if (pwm_cmd == 0x10) {
//     // this is a set duty cmd and is always given top priority
//     const auto new_duty = doc["duty"] | 0;
//     cmdKill();
//
//     rc = updateDuty(new_duty);
//   } else {
//     // handle the more complex commands that become running tasks
//     JsonObject cmd_obj = doc["cmd"];
//
//     // there's a command in the payload, attempt to create it and run it
//     if (cmd_obj && (_cmd = cmdCreate(cmd_obj))) {
//       rc = _cmd->run();
//     }
//   }
//
//   return rc;
// }
//
// bool PulseWidth::cmdKill() {
//   auto rc = false; // true = a cmd was killed
//
//   if (_cmd) {
//     // must kill the command before deleting it!!!
//     _cmd->kill();
//
//     delete _cmd;
//
//     _cmd = nullptr;
//
//     rc = true;
//   }
//
//   return rc;
// }

void PulseWidth::makeID() {
  if (_id[0]) return;

  constexpr size_t capacity = sizeof(_id) / sizeof(char);
  auto remaining = capacity;
  auto *p = _id;

  // very efficiently populate the prefix
  *p++ = 'p';
  *p++ = 'w';
  *p++ = 'm';
  *p++ = '/';

  remaining -= 4;
  memccpy(p, base_name, 0x00, remaining);

  // back up one, memccpy returns pointer to byte immediately after the copied 0x00
  p--;

  // add the divider between base name and pin identifier
  *p++ = ':';
  remaining -= p - _id;

  memccpy(p, description(), 0x00, remaining);
}

void PulseWidth::setBaseName(const char *base) {
  constexpr size_t capacity = sizeof(base_name) / sizeof(char);
  memccpy(base_name, base, 0x00, capacity);
}

bool PulseWidth::stop(uint32_t final_duty) {
  auto mode = channel_config[_num].speed_mode;
  auto channel = numToChannelMap[_num];
  _last_rc = ledc_stop(mode, channel, final_duty);

  if (_last_rc == ESP_OK) return true;

  // using TR = reading::Text;
  // TR::rlog("[%s] %s stop failed", esp_err_to_name(esp_rc), debug().get());

  return false;
}

IRAM_ATTR bool PulseWidth::updateDuty(uint32_t new_duty) {
  auto esp_rc = ESP_OK;

  const ledc_mode_t mode = channel_config[_num].speed_mode;
  const ledc_channel_t channel = numToChannelMap[_num];

  if (new_duty > _duty_max) return false;

  esp_rc = ledc_set_duty_and_update(mode, channel, new_duty, 0);

  if (esp_rc == ESP_OK) return true;

  return false;
}

//
// PRIVATE
//

// Command_t *PulseWidth::cmdCreate(JsonObject &cmd_obj) {
//   TextBuffer<15> type = cmd_obj["type"].as<const char *>();
//   Command_t *cmd = nullptr;
//
//   if (type.compare("random") == 0) {
//     cmd = new Random(PulseWidthDesc(address()), &_ledc_channel, cmd_obj);
//   }
//
//   return cmd;
// }

// STATIC

} // namespace device
