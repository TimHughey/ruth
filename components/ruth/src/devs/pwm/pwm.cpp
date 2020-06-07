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
#include "devs/pwm/cmds/basic.hpp"
#include "devs/pwm/cmds/random.hpp"
#include "devs/pwm/pwm.hpp"
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

void pwmDev::configureChannel() {
  gpio_set_level(_gpio_pin, 0);
  _last_rc = ledc_channel_config(&_ledc_channel);
}

bool pwmDev::cmd(JsonDocument &doc) {
  auto rc = false;
  Command_t *cmd = nullptr;
  JsonObject cmd_obj = doc["cmd"];

  if (cmd_obj) {
    // ok, there's a cmd.
    // get the name so an existing cmd with the same name can be stopped
    const char *cmd_name = cmd_obj["name"];

    // if the cmd name exists, erase it before allocating the new
    // cmd to minimize heap frag
    cmdErase(cmd_name);

    cmd = cmdCreate(cmd_obj);
    if (cmd) {
      _cmds.push_back(cmd);

      cmd->runIfNeeded();

      rc = true;
    }
  }

  return rc;
}

bool pwmDev::updateDuty(JsonDocument &doc) {
  const uint32_t new_duty = doc["duty"] | 0;

  if (_cmds.empty() == false) {
    for_each(_cmds.begin(), _cmds.end(), [this](Command_t *cmd) {
      if (cmd->running()) {
        cmd->kill();
      }
    });
  }

  return updateDuty(new_duty);
} // namespace ruth

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

Command_t *pwmDev::cmdCreate(JsonObject &cmd_obj) {
  string_t type = cmd_obj["type"];
  Command_t *cmd = nullptr;

  if (type.compare("basic") == 0) {
    cmd = new Basic(pwmDevDesc(addr()), &_ledc_channel, cmd_obj);

  } else if (type.compare("random") == 0) {
    cmd = new Random(pwmDevDesc(addr()), &_ledc_channel, cmd_obj);
  }

  return cmd;
}

bool pwmDev::cmdErase(const char *name) {
  bool rc = false;
  const string_t name_str = name;

  using std::find_if;

  auto found =
      find_if(_cmds.begin(), _cmds.end(), [this, name_str](Command_t *cmd) {
        if (name_str.compare(cmd->name()) == 0)
          return true;

        return false;
      });

  if (found != _cmds.end()) {
    Command_t *cmd = *found;

    // stop the command (if running)
    cmd->kill();

    // now it's safe to delete
    _cmds.erase(found);
    delete cmd;
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
