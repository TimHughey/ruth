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

#include "dev_pwm/pwm.hpp"

namespace ruth {

class StatusLED : public device::PulseWidth {
public:
  StatusLED(); // SINGLETON
  static void init();

  // control the brightness of the status led
  static void bright();
  static void brighter();
  static device::PulseWidth &device();
  static void dim();
  static void dimmer();
  static void percent(float p);
  static void off();
};
} // namespace ruth

#endif
