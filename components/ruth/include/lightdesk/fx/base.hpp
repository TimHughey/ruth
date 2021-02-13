/*
    lightdesk/fx/base.hpp -- LightDesk Effect Base Class
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

#ifndef _ruth_lightdesk_fx_base_hpp
#define _ruth_lightdesk_fx_base_hpp

#include "lightdesk/headunits/elwire.hpp"
#include "lightdesk/headunits/ledforest.hpp"
#include "lightdesk/headunits/pinspot/base.hpp"
#include "lightdesk/types.hpp"
#include "protocols/i2s.hpp"

namespace ruth {
namespace lightdesk {
namespace fx {

typedef struct FxConfig FxConfig_t;

struct FxConfig {

  I2s_t *i2s = nullptr;

  struct {
    PinSpot_t *main = nullptr;
    PinSpot_t *fill = nullptr;
  } pinspot;

  struct {
    ElWire_t *dance_floor = nullptr;
    ElWire_t *entry = nullptr;
  } elwire;

  LedForest_t *led_forest = nullptr;
  float runtime_max_secs = 17.0f;
};

typedef class FxBase FxBase_t;

class FxBase {
public:
  FxBase() {}
  FxBase(const FxType type) : _type(type) {}

  virtual ~FxBase() {}

  virtual void begin() {}

  bool execute() {
    if (checkComplexity()) {
      executeEffect();
    } else {
      completed();
    }

    return isComplete();
  }

  inline bool isComplete() {
    if (_type == fxNone) {
      _complete = true;
    } else if ((_runtime_secs == 0.0) && (_count == 0)) {
      _complete = true;
    } else if (((_elapsed.toSeconds() >= _runtime_secs) && (_count == 0)) ||
               _complete) {
      _complete = true;
    }

    return _complete;
  }

  static void setConfig(const FxConfig_t &cfg) { _cfg = cfg; }
  static void setRuntimeMax(const float max_secs) {
    _cfg.runtime_max_secs = max_secs;
  }

  FxType_t type() const { return _type; }

protected:
  inline bool checkComplexity() const {
    const float complexity = i2s()->complexityAvg();
    if ((_complexity_min > 0.0) && (complexity < _complexity_min)) {
      return false;
    }

    return true;
  }
  inline void completed() { _complete = true; }
  inline float &complexityMinimum() { return _complexity_min; }
  inline uint_fast16_t &count() { return _count; }
  inline uint_fast16_t &countMax() { return _count_max; }
  virtual void executeEffect() {}
  inline FxType_t fx() const { return _type; }

  inline I2s_t *i2s() const { return _cfg.i2s; }
  inline bool onetime() {
    const bool rc = _onetime;

    if (_onetime) {
      _onetime = false;
    }

    return rc;
  }
  inline void useCount(uint_fast16_t count) {
    _count_max = 10;
    _count = 10;
  }

  // headunits
  inline ElWire_t *elWireDanceFloor() const { return _cfg.elwire.dance_floor; }
  inline ElWire_t *elWireEntry() const { return _cfg.elwire.entry; }
  inline LedForest_t *ledForest() const { return _cfg.led_forest; }
  inline PinSpot_t *pinSpotFill() const { return _cfg.pinspot.fill; }
  inline PinSpot_t *pinSpotMain() const { return _cfg.pinspot.main; }

  // effect runtime helpers
  inline void runtime(const float secs) { _runtime_secs = secs; }
  inline float runtimeDefault() const { return _cfg.runtime_max_secs; }
  inline void runtimeUseDefault() { _runtime_secs = _cfg.runtime_max_secs; }
  inline float runtimePercent(const float percent) {
    return _runtime_secs * percent;
  }
  inline void runtimeReduceTo(const float percent) {
    _runtime_secs = _cfg.runtime_max_secs * percent;
  }

private:
  FxType_t _type = fxNone;
  static FxConfig_t _cfg;

  float _runtime_secs = 0.0;
  uint_fast16_t _count = 0;
  uint_fast16_t _count_max = 0;

  float _complexity_min = 0.0;

  bool _complete = false;
  bool _onetime = true;
  elapsedMillis _elapsed;
};

} // namespace fx
} // namespace lightdesk
} // namespace ruth

#endif
