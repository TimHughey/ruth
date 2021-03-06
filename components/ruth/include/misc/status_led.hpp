/*
    status_led.hpp -- Abstraction for ESP Status LED (pin 13)
    Copyright (C) 2019  Tim Hughey

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

#ifndef _ruth_status_led_hpp
#define _ruth_status_led_hpp

#include <sys/time.h>
#include <time.h>

#include <esp_log.h>

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_system.h>

namespace ruth {

typedef class StatusLED StatusLED_t;

class StatusLED {
public:
  StatusLED(){}; // SINGLETON
  // initialize the singleton
  static void init() { _instance_()->_init(); };

  // control the brightness of the status led
  static void bright() { _instance_()->_bright_(); };
  static void brighter() { _instance_()->_brighter_(); };
  static void dim() { _instance_()->_dim_(); };
  static void dimmer() { _instance_()->_dimmer_(); };
  static void duty(uint32_t duty) { _instance_()->_duty_(duty); };
  static void percent(float p) { _instance_()->_percent_(p); };
  static void off() { _instance_()->_off_(); };

private:
  static StatusLED_t *_instance_();
  void _init();

  void activate_duty();

  // private implementation of static instance methods
  void _bright_();
  void _brighter_();
  void _dim_();
  void _dimmer_();
  void _duty_(uint32_t new_duty);
  void _off_();
  void _percent_(float p);

private:
  uint32_t duty_max_ = 0b1111111111111; // 13 bits, 8191
  uint32_t duty_ = duty_max_ / 100;     // initial duty is very dim

  ledc_timer_config_t ledc_timer_ = {.speed_mode = LEDC_HIGH_SPEED_MODE,
                                     .duty_resolution = LEDC_TIMER_13_BIT,
                                     .timer_num = LEDC_TIMER_0,
                                     .freq_hz = 5000,
                                     .clk_cfg = LEDC_AUTO_CLK};

  ledc_channel_config_t ledc_channel_ = {.gpio_num = GPIO_NUM_13,
                                         .speed_mode = LEDC_HIGH_SPEED_MODE,
                                         .channel = LEDC_CHANNEL_0,
                                         .intr_type = LEDC_INTR_DISABLE,
                                         .timer_sel = LEDC_TIMER_0,
                                         .duty = duty_,
                                         .hpoint = 0};
};
} // namespace ruth

#endif
