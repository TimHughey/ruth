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

#include <algorithm>
#include <memory>
#include <string>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <time.h>

#include "devs/base.hpp"
#include "devs/pwm/pwm.hpp"
#include "devs/pwm/sequence/basic.hpp"
#include "local/types.hpp"
#include "net/network.hpp"

using std::unique_ptr;

namespace ruth {

const char *pwmDev::pwmDevDesc(const DeviceAddress_t &addr) {
  switch (addr.firstAddressByte()) {
  case 0x01:
    return (const char *)"pin:1";

  case 0x02:
    return (const char *)"pin:2";

  case 0x03:
    return (const char *)"pin:3";

  case 0x04:
    return (const char *)"pin:4";

  default:
    return (const char *)"unknown";
  }
}

// construct a new pwmDev with a known address and compute the id
pwmDev::pwmDev(DeviceAddress_t &num) : Device(num) {
  unique_ptr<char[]> id(new char[_pwm_max_id_len + 1]);

  _gpio_pin = mapNumToGPIO(num);

  _ledc_channel.gpio_num = _gpio_pin;
  _ledc_channel.channel = mapNumToChannel(num);

  setDescription(pwmDevDesc(num));

  snprintf(id.get(), _pwm_max_id_len, "pwm/%s.%s", Net::hostname(),
           pwmDevDesc(num));

  setID(id.get());
};

uint8_t pwmDev::devAddr() { return firstAddressByte(); };

void pwmDev::configureChannel() {
  gpio_set_level(_gpio_pin, 0);
  _last_rc = ledc_channel_config(&_ledc_channel);
}

bool pwmDev::sequence(JsonDocument &doc) {
  JsonObject obj = doc["seq"];

  if (obj.isNull())
    return false;

  // retrieve the sequence name to erase duplicate / replacement
  // the name will be passed the Sequence constructor (which will copy it)
  const char *seq_name = obj["name"];

  // if the sequence name exists, erase it before allocating the new
  // sequence to minimize heap frag
  bool erased = eraseSequence(seq_name);

  Sequence_t *seq = new Basic(pwmDevDesc(addr()), &_ledc_channel, obj);

  _sequences.push_back(seq);

  if (erased) {
    ST::rlog("pwmDev replaced sequence name=\"%s\"", seq_name);
  } else {
    ST::rlog("pwmDev created sequence name=\"%s\"", seq_name);
  }

  if (seq->active()) {
    seq->run();
  }

  return true;
}

bool pwmDev::updateDuty(JsonDocument &doc) {
  const uint32_t new_duty = doc["duty"] | 0;

  return updateDuty(new_duty);
}

bool pwmDev::updateDuty(uint32_t new_duty) {
  auto esp_rc = ESP_OK;

  const ledc_mode_t mode = _ledc_channel.speed_mode;
  const ledc_channel_t channel = _ledc_channel.channel;

  if (new_duty > _duty_max)
    return false;

  writeStart();
  esp_rc = ledc_set_duty_and_update(mode, channel, new_duty, 0);
  writeStop();

  if (esp_rc == ESP_OK) {
    _duty = new_duty;
    return true;
  }

  return false;
}

//
// PRIVATE
//

bool pwmDev::eraseSequence(const char *name) {
  bool rc = false;
  const string_t name_str = name;

  using std::find_if;

  auto found = find_if(_sequences.begin(), _sequences.end(),
                       [this, name_str](Sequence_t *seq) {
                         if (name_str.compare(seq->name()) == 0)
                           return true;

                         return false;
                       });

  if (found != _sequences.end()) {
    _sequences.erase(found);

    Sequence_t *seq = *found;

    delete seq;
    rc = true;
  }

  return rc;
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
           _duty, _ledc_channel.channel, _gpio_pin, esp_err_to_name(_last_rc));

  return move(debug_str);
}
} // namespace ruth
