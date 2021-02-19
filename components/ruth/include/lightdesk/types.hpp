/*
    lightdesk/types.hpp - Ruth Light Desk Types
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

#ifndef _ruth_lightdesk_types_hpp
#define _ruth_lightdesk_types_hpp

#include <cstdint>

#include "lightdesk/enums.hpp"
#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "misc/tracked.hpp"
#include "misc/valminmax.hpp"

namespace ruth {
namespace lightdesk {

typedef class HeadUnit HeadUnit_t;

typedef enum ColorPart ColorPart_t;
typedef enum ElWireFunction ElWireFunction_t;
typedef enum FxType FxType_t;
typedef enum LightDeskMode LightDeskMode_t;
typedef enum PinSpotFunction PinSpotFunction_t;

typedef float Strobe_t;
typedef struct DmxStats DmxStats_t;
typedef struct I2sStats I2sStats_t;
typedef struct FxStats FxStats_t;
typedef struct LightDeskStats LightDeskStats_t;

struct DmxStats {
  float fps = 0.0;
  uint64_t busy_wait = 0;

  struct {
    uint_fast64_t us;
    uint64_t count = 0;
    uint64_t shorts = 0;
    elapsedMicrosTracked white_space_us;
    float fps_expected = 0.0;

    elapsedMicrosTracked update_us;
    elapsedMicrosTracked prepare_us;
  } frame;

  ValMinMaxFloat_t tx_ms;
};

struct FxStats {
  FxType_t active = lightdesk::fxNone;
  FxType_t next = lightdesk::fxNone;
  FxType_t prev = lightdesk::fxNone;
};

struct I2sStats {

  struct {
    CountPerInterval raw_bps;
    float samples_per_sec;
    CountPerInterval fft_per_sec;
  } rate;

  ValMinMax<int_fast32_t> raw_val_left;
  ValMinMax<int_fast32_t> raw_val_right;
  ValMinMaxFloat_t dB;

  struct {
    float instant = 0;
    float avg7sec = 0;
  } complexity;

  struct {
    float freq_bin_width = 0;
    float dB_floor = 0;
    float bass_dB_floor = 0;
    float complexity_dB_floor = 0;
  } config;

  struct {
    elapsedMicrosTracked fft_us;
    elapsedMicrosTracked rx_us;
  } elapsed;

  struct {
    float freq = 0.0;
    float dB = 0.0;
  } mpeak;
};

struct LightDeskStats {

  const char *mode = nullptr;

  DmxStats_t dmx;
  FxStats fx;
  I2sStats_t i2s;

  bool ac_power = false;

  struct {
    uint32_t dance_floor = 0;
    uint32_t entry = 0;
  } elwire;

  uint32_t led_forest = 0;
};

const char *fxDesc(const FxType_t fx);
const char *modeDesc(const LightDeskMode_t mode);

} // namespace lightdesk
} // namespace ruth

#endif
