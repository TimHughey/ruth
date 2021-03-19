/*
    lightdesk/headunits/pwm_base.hpp - Ruth LightDesk Head Unit Pwm Base
    Copyright (C) 2021  Tim Hughey

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

#ifndef _ruth_lightdesk_headunits_pwm_base_hpp
#define _ruth_lightdesk_headunits_pwm_base_hpp

#include <driver/gpio.h>
#include <driver/ledc.h>

#include "devs/pwm/pwm.hpp"
#include "lightdesk/headunit.hpp"
#include "readings/text.hpp"

namespace ruth {
namespace lightdesk {

class PulseWidthHeadUnit : public HeadUnit, public PwmDevice {
public:
  PulseWidthHeadUnit(uint8_t num) : PwmDevice(num) {
    PulseWidthHeadUnit::configureTimer();
    updateDuty(0);
    configureChannel();
  }

  virtual ~PulseWidthHeadUnit() { stop(); }

public:
  static void configureTimer() {
    if (_timer_configured == false) {
      auto timer_rc = ledc_timer_config(&_ledc_timer);

      if (timer_rc == ESP_OK) {
        _timer_configured = true;
      } else {
        using TR = reading::Text;
        TR::rlog("[%s] %s", esp_err_to_name(timer_rc), __PRETTY_FUNCTION__);
      }

      PwmDevice::allOff();
    }
  }

  virtual void dark() override { updateDuty(0); }

  virtual void handleMsg(const JsonObject &obj) override = 0;
  static void preStart() {
    configureTimer();
    allOff();
  }

private:
  // class members, defined and initialized in misc/statics.cpp
  static bool _timer_configured;
  static const ledc_timer_config_t _ledc_timer;
};

} // namespace lightdesk
} // namespace ruth

#endif
