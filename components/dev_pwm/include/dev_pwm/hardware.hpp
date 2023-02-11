//  Ruth
//  Copyright (C) 2020  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "esp_err.h"
#include <memory>

namespace ruth {
namespace pwm {
class Hardware {
public:
  Hardware(uint8_t pin_num);
  ~Hardware() = default;

  static esp_err_t allOff();

  uint32_t duty(bool *changed = nullptr);
  inline uint32_t dutyMax() const { return _duty_max; };
  inline uint32_t dutyMin() const { return _duty_min; };
  uint32_t dutyPercent(const float percent) const {
    return uint32_t((float)_duty_max * (percent / 100));
  }
  inline esp_err_t lastRC() const { return _last_rc; }
  inline bool off() { return updateDuty(dutyMin()); }
  inline bool on() { return updateDuty(dutyMax()); }

  inline uint8_t pinNum() const { return _pin_num; }

  inline Hardware *self() { return this; }
  const char *shortName() const;
  bool stop(uint32_t final_duty);

  bool updateDuty(uint32_t duty);

private:
  void ensureChannel(uint8_t num);
  void ensureTimer();

private:
  static bool _timer_configured;
  static bool _channel_configured[5];

  static uint64_t _numToGpioMap[5];
  static uint32_t _numToChannelMap[5];

  uint8_t _pin_num;

  static constexpr uint32_t _duty_max = 0x1fff;
  static constexpr uint32_t _duty_min = 0;
  uint32_t _duty;

  esp_err_t _last_rc = ESP_OK;
};

} // namespace pwm
} // namespace ruth
