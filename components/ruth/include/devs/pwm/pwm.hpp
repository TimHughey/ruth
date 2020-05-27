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

#include <memory>
#include <string>

#include <driver/gpio.h>
#include <driver/ledc.h>

#include "devs/base.hpp"
#include "external/ArduinoJson.h"

using std::unique_ptr;

namespace ruth {

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
  ledc_channel_t channel() { return ledc_channel_.channel; };
  ledc_mode_t speedMode() { return ledc_channel_.speed_mode; };
  uint32_t dutyMax() { return duty_max_; };
  uint32_t dutyMin() { return duty_min_; };
  gpio_num_t gpioPin() { return gpio_pin_; };

  // sequence support
  void sequenceStart() { running_ = true; }
  void sequenceEnd() { running_ = false; }
  bool needsRun() { return running_; }
  bool run();

  bool updateDuty(uint32_t duty);

  esp_err_t lastRC() { return last_rc_; };

  // info / debug functions
  const unique_ptr<char[]> debug();

private:
  static const uint32_t pwm_max_addr_len_ = 1;
  static const uint32_t pwm_max_id_len_ = 40;
  static const uint32_t duty_max_ = 0x1fff;
  static const uint32_t duty_min_ = 0;

  gpio_num_t gpio_pin_;
  uint32_t duty_ = 0; // default to zero (off)
  esp_err_t last_rc_ = ESP_OK;

  // flag that signals the device is running a sequence
  bool running_ = false;

  ledc_channel_config_t ledc_channel_ = {.gpio_num = 0, // set by constructor
                                         .speed_mode = LEDC_HIGH_SPEED_MODE,
                                         // channel default value is changed
                                         // by constructor
                                         .channel = LEDC_CHANNEL_1,
                                         .intr_type = LEDC_INTR_DISABLE,
                                         .timer_sel = LEDC_TIMER_1,
                                         .duty = duty_,
                                         .hpoint = 0};
};
} // namespace ruth

#endif // pwm_dev_hpp
