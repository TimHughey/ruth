/*
    devs/pinspot/base.hpp - Ruth Pin Spot Device
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

#ifndef _ruth_pinspot_device_base_hpp
#define _ruth_pinspot_device_base_hpp

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include "devs/dmx/headunit.hpp"
#include "devs/dmx/pinspot/color.hpp"
#include "devs/dmx/pinspot/fader.hpp"
#include "lightdesk/fx_defs.hpp"
#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "protocols/dmx.hpp"

namespace ruth {
namespace lightdesk {

typedef class PinSpot PinSpot_t;

class PinSpot : public HeadUnit {
public:
  PinSpot(uint16_t address = 1);
  ~PinSpot();

  void framePrepare();

  // modes
  void autoRun(Fx_t fx);
  void black();
  const Color_t &color() const { return _color; }
  void color(int r, int g, int b, int w) { color(Color(r, g, b, w)); }
  void color(const Color_t &color, float strobe = 0.0);
  void dark();
  void fadeTo(const Color_t &color, float secs = 1.0, float accel = 0.0);
  void fadeTo(const FaderOpts_t &opts);

  void stats(PinSpotStats_t &stats) const { stats = _stats; }

  typedef enum { AUTORUN = 0x3000, DARK, COLOR, FADER, HOLD } Mode_t;

private:
  // functions
  uint8_t autorunMap(Fx_t fx) const;

  inline bool faderFinished() const { return _fader.finished(); };
  void faderMove();
  void faderStart(const FaderOpts &opts);

  void frameUpdate();

private:
  esp_err_t _init_rc = ESP_FAIL;

  // update frame spinlock
  // portMUX_TYPE _spinlock = portMUX_INITIALIZER_UNLOCKED;

  Mode_t _mode = DARK;

  Color_t _color;
  uint8_t _strobe = 0;
  uint8_t _strobe_max = 104;
  Fx_t _fx = fxNone;

  PinSpotStats_t _stats;

  Fader_t _fader;
};
} // namespace lightdesk
} // namespace ruth

#endif
