/*
    lightdesk/fx.hpp - Ruth Light Desk Fx
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

#ifndef _ruth_lightdesk_fx_hpp
#define _ruth_lightdesk_fx_hpp

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include "devs/dmx/pinspot/base.hpp"
#include "lightdesk/fx_defs.hpp"
#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "misc/random.hpp"
#include "protocols/dmx.hpp"

namespace ruth {
namespace lightdesk {

typedef class LightDeskFx LightDeskFx_t;

class LightDeskFx {
public:
  LightDeskFx(PinSpot_t *main, PinSpot_t *fill);

  void execute(Fx_t next_fx);
  bool execute(bool *finished = nullptr);

  float intervalDefault() const { return _fx_interval_default; }
  void intervalDefault(const float interval) {
    _fx_interval_default = interval;
  }

  float nextTimerInterval() const { return _fx_interval; }

  void start();

  void stats(LightDeskFxStats_t &stats) {
    _stats.active = _fx_active;
    _stats.next = _fx_next;
    _stats.interval_default = _fx_interval_default;
    _stats.interval = _fx_interval;
    _stats.object_size = sizeof(LightDeskFx_t);

    stats = _stats;
  }

private:
  inline uint32_t &fxActiveCount() { return _fx_active_count; }

  // scale < 1.00 -> reduction
  // scale == 1.00 -> no change
  // scale > 1.00 -> increase
  inline void intervalChange(float scale) {
    const float change = _fx_interval * scale;

    if (scale < 1.00) {
      _fx_interval = _fx_interval - change;
    } else if (scale > 1.00) {
      _fx_interval = _fx_interval + change;
    }
  }

  inline float intervalPercent(float percent) const {
    return _fx_interval * percent;
  }

  inline void intervalReset() { _fx_interval = _fx_interval_default; }

  // Fx specific Functions
  void basic(Fx_t fx);
  bool crossFadeFast();
  bool colorBars();
  void fullSpectrumCycle();
  void primaryColorsCycle();
  void simpleStrobe();
  void soundFastStrobe();
  void soundWashed();
  void whiteFadeInOut();

private:
  PinSpot_t *_main = nullptr;
  PinSpot_t *_fill = nullptr;

  Fx_t _fx_active = fxNone;
  uint32_t _fx_active_count = 0;
  bool _fx_finished = true;
  float _fx_interval = 0.0f;
  float _fx_interval_default = 17.0f;
  Fx_t _fx_next = fxNone;
  Fx_t _fx_prev = fxNone;

  LightDeskFxStats_t _stats;
};

} // namespace lightdesk
} // namespace ruth

#endif
