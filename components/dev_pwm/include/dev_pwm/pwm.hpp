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

#include <memory>

#include <freertos/FreeRTOS.h>

namespace device {

class PulseWidth {

public:
  PulseWidth(uint8_t num);

  static esp_err_t allOff();
  uint8_t devAddr() { return _num; };
  bool available() const { return true; }
  const char *description() const;
  const char *id() const { return _id; }

  void makeID();
  uint32_t duty() const;
  uint32_t dutyMax() const { return _duty_max; };
  uint32_t dutyMin() const { return _duty_min; };
  uint32_t dutyPercent(const float percent) const { return uint32_t((float)_duty_max * (percent / 100)); }
  bool off() { return updateDuty(dutyMin()); }
  bool on() { return updateDuty(dutyMax()); }

  static void setBaseName(const char *base);
  void setCommand(const char *cmd);
  bool stop(uint32_t final_duty = 0);

  bool updateDuty(uint32_t duty);

  // info / debug functions
  const std::unique_ptr<char[]> debug();

private:
  void ensureChannel(uint8_t num);
  void ensureTimer();

private:
  static bool _timer_configured;
  static bool _channel_configured[5];

  static uint64_t _numToGpioMap[5];
  static uint32_t _numToChannelMap[5];

  static constexpr uint32_t _duty_max = 0x1fff;
  static constexpr uint32_t _duty_min = 0;
  static UBaseType_t _priority;

  uint32_t _num = 0;
  char _id[32] = {};
  char _cmd[32] = {};

  // Command_t *_cmd = nullptr;

  esp_err_t _last_rc = ESP_OK;

private:
  // Command_t *cmdCreate(JsonObject &obj);
};
} // namespace device

#endif // pwm_dev_hpp
