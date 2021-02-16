/*
    lightdesk/headunits/discoball.hpp - Ruth LightDesk Headunit Disco Ball
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

#ifndef _ruth_lightdesk_headunits_discoball_hpp
#define _ruth_lightdesk_headunits_discoball_hpp

#include "lightdesk/headunits/pwm.hpp"

namespace ruth {
namespace lightdesk {

typedef class DiscoBall DiscoBall_t;

class DiscoBall : public PulseWidthHeadUnit {

public:
  DiscoBall(uint8_t pwm_num) : PulseWidthHeadUnit(pwm_num) {
    config.max = unitPercent(0.75);
  }

public: // effects
  inline void spin() { fixed(0.63); }
  inline void still() { dark(); }
};

} // namespace lightdesk
} // namespace ruth

#endif
