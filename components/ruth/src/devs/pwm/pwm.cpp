/*
    pwm.cpp - Ruth PWM Device
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

#include <memory>
#include <string>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <sys/time.h>
#include <time.h>

#include "devs/base.hpp"
#include "devs/pwm/pwm.hpp"
#include "local/types.hpp"
#include "net/network.hpp"

using std::unique_ptr;

namespace ruth {

const char *pwmDev::pwmDevDesc(const DeviceAddress_t &addr) {
  switch (addr.firstAddressByte()) {
  case 0x01:
    return (const char *)"pin:1";
    break;

  case 0x02:
    return (const char *)"pin:2";
    break;

  case 0x03:
    return (const char *)"pin:3";
    break;

  case 0x04:
    return (const char *)"pin:4";
    break;

  default:
    return (const char *)"unknown";
    break;
  }
}

// construct a new pwmDev with a known address and compute the id
pwmDev::pwmDev(DeviceAddress_t &num) : Device(num) {
  unique_ptr<char[]> id(new char[pwm_max_id_len_ + 1]);

  gpio_pin_ = mapNumToGPIO(num);

  ledc_channel_.gpio_num = gpio_pin_;
  ledc_channel_.channel = mapNumToChannel(num);

  setDescription(pwmDevDesc(num));

  snprintf(id.get(), pwm_max_id_len_, "pwm/%s.%s", Net::getName().c_str(),
           pwmDevDesc(num));

  setID(id.get());
};

uint8_t pwmDev::devAddr() { return firstAddressByte(); };

void pwmDev::configureChannel() {
  gpio_set_level(gpio_pin_, 0);
  last_rc_ = ledc_channel_config(&ledc_channel_);
}

bool pwmDev::run() {
  if (running_ == false) {
    return false;
  }

  // device ran, return true
  return true;
}

bool pwmDev::updateDuty(JsonDocument &doc) {
  auto esp_rc = ESP_OK;

  const ledc_mode_t mode = ledc_channel_.speed_mode;
  const ledc_channel_t channel = ledc_channel_.channel;

  writeStart();

  ledc_set_duty_and_update(mode, channel, doc["duty"], 0);

  writeStop();

  duty_ = doc["duty"];

  return (esp_rc == ESP_OK) ? true : false;
}

// STATIC
void pwmDev::allOff() {
  gpio_num_t pins[4] = {GPIO_NUM_32, GPIO_NUM_15, GPIO_NUM_33, GPIO_NUM_27};

  // ensure all pins to be used as PWM are off
  gpio_config_t pins_cfg;

  pins_cfg.pin_bit_mask = PWM_GPIO_PIN_SEL;
  pins_cfg.mode = GPIO_MODE_OUTPUT;
  pins_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  pins_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  pins_cfg.intr_type = GPIO_INTR_DISABLE;

  gpio_config(&pins_cfg);

  for (int i = 0; i < 4; i++) {
    gpio_set_level(pins[i], 0);
  }
}

// STATIC
ledc_channel_t pwmDev::mapNumToChannel(const DeviceAddress_t &num) {
  switch (num.firstAddressByte()) {
  case 0x01:
    // NOTE: LEDC_CHANNEL_0 is used for the onboard red status led
    return (LEDC_CHANNEL_2);

  case 0x02:
    return (LEDC_CHANNEL_3);

  case 0x03:
    return (LEDC_CHANNEL_4);

  case 0x04:
    return (LEDC_CHANNEL_5);

  default:
    return (LEDC_CHANNEL_5);
  }
}

// STATIC
gpio_num_t pwmDev::mapNumToGPIO(const DeviceAddress_t &num) {
  switch (num.firstAddressByte()) {
  case 0x01:
    return GPIO_NUM_32;

  case 0x02:
    return GPIO_NUM_15;

  case 0x03:
    return GPIO_NUM_33;

  case 0x04:
    return GPIO_NUM_27;

  default:
    return GPIO_NUM_32;
  }
}

const unique_ptr<char[]> pwmDev::debug() {
  const auto max_len = 127;
  unique_ptr<char[]> debug_str(new char[max_len + 1]);

  snprintf(debug_str.get(), max_len,
           "pwmDev(%s duty=%d channel=%d pin=%d last_rc=%s)", id().c_str(),
           duty_, ledc_channel_.channel, gpio_pin_, esp_err_to_name(last_rc_));

  return move(debug_str);
}
} // namespace ruth
