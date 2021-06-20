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

#ifndef _ruth_pwm_dev_hpp
#define _ruth_pwm_dev_hpp

#include <cstdlib>

#include <driver/gpio.h>
#include <driver/ledc.h>

#include "ArduinoJson.h"
#include "dev_base/base.hpp"

namespace device {

static constexpr uint64_t pwm_gpio_pin_sel = (GPIO_SEL_32 | GPIO_SEL_15 | GPIO_SEL_33 | GPIO_SEL_27);
// #define PWM_GPIO_PIN_SEL (GPIO_SEL_32 | GPIO_SEL_15 | GPIO_SEL_33 | GPIO_SEL_27)

class PulseWidth : public Base {
public:
  PulseWidth() {}
  // static const char *DeviceDesc(const Address &addr);
  static gpio_num_t mapNumToGPIO(const Address &num);
  static ledc_channel_t mapNumToChannel(const Address &num);
  static void allOff();

public:
  PulseWidth(uint8_t num);

  uint8_t devAddr() { return firstAddressByte(); };

  // override the base class function
  // pwm devices are always available provided the engine is enabled
  bool available() const override { return true; }

  void makeID() override;

  // ledc channel
  void configureChannel();
  ledc_channel_t channel() { return _ledc_channel.channel; };
  ledc_mode_t speedMode() { return _ledc_channel.speed_mode; };

  inline uint32_t duty() {
    _duty = ledc_get_duty(speedMode(), channel());

    return _duty;
  }

  uint32_t dutyMax() const { return _duty_max; };
  uint32_t dutyMin() const { return _duty_min; };
  uint32_t dutyPercent(const float percent) const { return uint32_t((float)_duty_max * percent); }

  gpio_num_t gpioPin() { return _gpio_pin; };

  // primary entry point for all cmds except duty
  // bool cmd(uint32_t pwm_cmd, JsonDocument &doc);
  // bool cmdKill();

  bool off() { return updateDuty(dutyMin()); }
  bool on() { return updateDuty(dutyMax()); }

  bool stop(uint32_t duty = 0) {
    auto rc = true;
    auto const esp_rc = ledc_stop(speedMode(), channel(), duty);

    if (esp_rc != ESP_OK) {
      rc = false;

      // using TR = reading::Text;
      // TR::rlog("[%s] %s stop failed", esp_err_to_name(esp_rc), debug().get());
    }

    return rc;
  }

  bool updateDuty(uint32_t duty);
  bool updateDuty(JsonDocument &doc);

  esp_err_t lastRC() { return _last_rc; };

  // info / debug functions
  const unique_ptr<char[]> debug() override;

private:
  static constexpr uint32_t _duty_max = 0x1fff;
  static constexpr uint32_t _duty_min = 0;
  static char *_device_base;
  static UBaseType_t _priority;

  // Command_t *_cmd = nullptr;

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
  // Command_t *cmdCreate(JsonObject &obj);
};
} // namespace device

#endif // pwm_dev_hpp
