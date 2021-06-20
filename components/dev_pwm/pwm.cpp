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

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// #include "dev_pwm/cmd_random.hpp"
#include "dev_pwm/pwm.hpp"

namespace device {

char *PulseWidth::_device_base = nullptr;

// construct a new PulseWidth with a known address and compute the id
PulseWidth::PulseWidth(uint8_t num) : device::Base(num) {
  _gpio_pin = mapNumToGPIO(num);

  _ledc_channel.gpio_num = _gpio_pin;
  _ledc_channel.channel = mapNumToChannel(num);

  switch (singleByteAddress()) {
  case 0x01:
    _desc = "pin:1";
    break;

  case 0x02:
    _desc = "pin:2";
    break;

  case 0x03:
    _desc = "pin:3";
    break;

  case 0x04:
    _desc = "pin:4";
    break;
  }

  makeID();
}

void PulseWidth::makeID() { setID("pwm/%s.%s", _device_base, description()); }

void PulseWidth::configureChannel() {
  gpio_set_level(_gpio_pin, 0);
  _last_rc = ledc_channel_config(&_ledc_channel);
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

IRAM_ATTR bool PulseWidth::updateDuty(uint32_t new_duty) {
  auto esp_rc = ESP_OK;

  const ledc_mode_t mode = _ledc_channel.speed_mode;
  const ledc_channel_t channel = _ledc_channel.channel;

  if (new_duty > _duty_max) return false;

  writeStart();
  esp_rc = ledc_set_duty_and_update(mode, channel, new_duty, 0);
  writeStop();

  if (esp_rc == ESP_OK) {
    _duty = new_duty;
    return true;
  } else {
    printf("%s failed duty %u\n", id(), new_duty);
  }

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
void PulseWidth::allOff() {
  gpio_num_t pins[4] = {GPIO_NUM_32, GPIO_NUM_15, GPIO_NUM_33, GPIO_NUM_27};

  // ensure all pins to be used as PWM are off
  gpio_config_t pins_cfg;

  pins_cfg.pin_bit_mask = pwm_gpio_pin_sel;
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
ledc_channel_t PulseWidth::mapNumToChannel(const Address &num) {
  switch (num.singleByte()) {
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
gpio_num_t PulseWidth::mapNumToGPIO(const Address &num) {
  switch (num.singleByte()) {
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

const unique_ptr<char[]> PulseWidth::debug() {
  const auto max_len = 127;
  unique_ptr<char[]> debug_str(new char[max_len + 1]);

  snprintf(debug_str.get(), max_len, "PulseWidth(%s duty=%d channel=%d pin=%d last_rc=%s)", id(), _duty,
           _ledc_channel.channel, _gpio_pin, esp_err_to_name(_last_rc));

  return move(debug_str);
}
} // namespace device
