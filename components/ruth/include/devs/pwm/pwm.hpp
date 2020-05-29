/*
    pwm.hpp - Ruth PWM Device
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

#ifndef _ruth_pwm_dev_hpp
#define _ruth_pwm_dev_hpp

#include <list>
#include <memory>
#include <string>

#include <driver/gpio.h>
#include <driver/ledc.h>

#include "devs/base.hpp"
#include "devs/pwm/sequence/sequence.hpp"
#include "external/ArduinoJson.h"

using std::unique_ptr;

namespace ruth {

using std::list;

using namespace pwm;

typedef class pwmDev pwmDev_t;

#define PWM_GPIO_PIN_SEL (GPIO_SEL_32 | GPIO_SEL_15 | GPIO_SEL_33 | GPIO_SEL_27)

class pwmDev : public Device {
public:
  pwmDev() {}
  static const char *pwmDevDesc(const DeviceAddress_t &addr);
  static gpio_num_t mapNumToGPIO(const DeviceAddress_t &num);
  static ledc_channel_t mapNumToChannel(const DeviceAddress_t &num);
  static void allOff();

public:
  pwmDev(DeviceAddress_t &num);
  uint8_t devAddr();

  void configureChannel();
  ledc_channel_t channel() { return _ledc_channel.channel; };
  ledc_mode_t speedMode() { return _ledc_channel.speed_mode; };
  uint32_t dutyMax() { return _duty_max; };
  uint32_t dutyMin() { return _duty_min; };
  gpio_num_t gpioPin() { return _gpio_pin; };

  // sequence support
  bool sequence(JsonDocument &doc);

  bool updateDuty(uint32_t duty);
  bool updateDuty(JsonDocument &doc);

  esp_err_t lastRC() { return _last_rc; };

  // info / debug functions
  const unique_ptr<char[]> debug();

private:
  static const uint32_t _pwm_max_id_len = 40;
  static const uint32_t _duty_max = 0x1fff;
  static const uint32_t _duty_min = 0;

  list<Sequence_t *> _sequences = {};

  gpio_num_t _gpio_pin;
  uint32_t _duty = 0; // default to zero (off)
  esp_err_t _last_rc = ESP_OK;

  ledc_channel_config_t _ledc_channel = {.gpio_num = 0, // set by constructor
                                         .speed_mode = LEDC_HIGH_SPEED_MODE,
                                         // channel default value is changed
                                         // by constructor
                                         .channel = LEDC_CHANNEL_1,
                                         .intr_type = LEDC_INTR_DISABLE,
                                         .timer_sel = LEDC_TIMER_1,
                                         .duty = _duty,
                                         .hpoint = 0};

private:
  bool eraseSequence(const char *name);
};
} // namespace ruth

#endif // pwm_dev_hpp
