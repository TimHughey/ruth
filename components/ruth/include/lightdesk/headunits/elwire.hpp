/*
    lightdesk/headunits/elwire.hpp - Ruth LightDesk Headunit EL Wire
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

#ifndef _ruth_lightdesk_headunits_elwire_hpp
#define _ruth_lightdesk_headunits_elwire_hpp

#include <array>

#include "lightdesk/headunits/pwm.hpp"

namespace ruth {
namespace lightdesk {

class ElWire : public PulseWidthHeadUnit {

public:
  ElWire(uint8_t pwm_num) : PulseWidthHeadUnit(pwm_num) {
    snprintf(_id.data(), _id.size(), "EL%u", pwm_num);
  }

  void handleMsg(const JsonObject &obj) override {
    const uint32_t duty = obj[_id.data()] | 0;

    updateDuty(duty);
  }

private:
  std::array<char, 7> _id;
};

} // namespace lightdesk
} // namespace ruth

#endif
