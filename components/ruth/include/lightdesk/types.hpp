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
#include <limits>

#include "lightdesk/enums.hpp"
#include "local/types.hpp"
#include "misc/elapsed.hpp"

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
  using uint32_limit = std::numeric_limits<uint_fast32_t>;

  float fps = 0.0;
  uint64_t busy_wait = 0;
  uint64_t frame_update_us = 0;

  struct {
    uint64_t us = 0;
    uint64_t count = 0;
    uint64_t shorts = 0;
    uint32_t white_space_us = 0;
    float fps_expected = 0.0;

    struct {
      uint64_t curr = 0;
      uint64_t min = 0;
      uint64_t max = 0;
    } update;

    struct {
      uint_fast32_t min = uint32_limit::max();
      uint_fast32_t curr = 0.0;
      uint_fast32_t max = uint32_limit::min();
    } prepare;

  } frame;

  struct {
    float curr = 0.0f;
    float min = 0.0f;
    float max = 0.0f;
  } tx;
};

struct FxStats {
  FxType_t active = lightdesk::fxNone;
  FxType_t next = lightdesk::fxNone;
  FxType_t prev = lightdesk::fxNone;
};

struct I2sStats {
  using float_limit = std::numeric_limits<float>;

  struct {
    uint32_t byte_count = 0;
    size_t fft_us_idx = 0;
    size_t rx_us_idx = 0;
  } temp;

  struct {
    float raw_bps = 0;
    float samples_per_sec = 0;
    float fft_per_sec = 0;
  } rate;

  struct {
    int32_t min24 = INT_MAX;
    int32_t max24 = min24 * -1;
  } raw_val;

  struct {
    elapsedMillis window;
    float min = float_limit::max();
    float max = float_limit::min();
  } magnitude;

  struct {
    float instant = 0;
    float avg7sec = 0;
  } complexity;

  struct {
    float freq_bin_width = 0;
    float mag_floor = 0;
    float bass_mag_floor = 0;
    float complexity_floor = 0;
  } config;

  struct {
    uint32_t fft_us[6] = {};
    float fft_avg_us = 0;

    uint32_t rx_us[6] = {};
    float rx_avg_us = 0;
  } durations;

  struct {
    float freq = 0.0;
    float mag = 0.0;
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
