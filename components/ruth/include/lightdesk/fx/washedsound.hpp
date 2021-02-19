/*
    lightdesk/fx/washsound.hpp -- PinSpot Auto Sound with White Fade
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

#ifndef _ruth_lightdesk_fx_washedsound_hpp
#define _ruth_lightdesk_fx_washedsound_hpp

#include "lightdesk/fx/base.hpp"

namespace ruth {
namespace lightdesk {
namespace fx {

class WashedSound : public FxBase {
public:
  WashedSound() : FxBase(fxWashedSound) {
    runtimeReduceTo(0.50f);
    complexityMinimum() = 75.0;
  }

  void executeEffect() {
    if (onetime()) {
      const FaderOpts fo{.origin = Color::white(),
                         .dest = Color::black(),
                         .travel_secs = 3.1f,
                         .use_origin = true};

      pinSpotFill()->fadeTo(fo);
      pinSpotMain()->autoRun(fxFastStrobeSound);
    }
  }

private:
};

} // namespace fx
} // namespace lightdesk
} // namespace ruth

#endif
