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

typedef enum ColorPart ColorPart_t;
typedef enum Fx Fx_t;
typedef enum LightDeskMode LightDeskMode_t;
typedef enum PinSpotFunction PinSpotFunction_t;

typedef float Strobe_t;
typedef struct DmxStats DmxStats_t;
typedef struct FxStats FxStats_t;
typedef struct I2sStats I2sStats_t;
typedef struct LightDeskStats LightDeskStats_t;
typedef struct PinSpotStats PinSpotStats_t;

struct DmxStats {
  float fps = 0.0;
  uint64_t busy_wait = 0;
  uint64_t frame_update_us = 0;
  size_t object_size = 0;

  struct {
    uint64_t us = 0;
    uint64_t count = 0;
    uint64_t shorts = 0;
    uint32_t white_space_us = 0;

    struct {
      uint64_t curr = 0;
      uint64_t min = 0;
      uint64_t max = 0;
    } update;
  } frame;

  struct {
    float curr = 0.0f;
    float min = 0.0f;
    float max = 0.0f;
  } tx;
};

struct FxStats {
  struct {
    uint64_t basic = 0;
    Fx_t active = lightdesk::fxNone;
    Fx_t next = lightdesk::fxNone;
    Fx_t prev = lightdesk::fxNone;
  } fx;

  struct {
    float base = 0.0f;
    float current = 0.0f;
    float min = 9999.9f;
    float max = 0.0f;
  } interval;

  float major_peak_roc_floor = 0.0;
  size_t object_size = 0;
};

struct I2sStats {
  using numerical_limits = std::numeric_limits<float>;

  struct {
    uint32_t byte_count = 0;
    size_t fft_us_idx = 0;
    size_t rx_us_idx = 0;
  } temp;

  struct {
    float raw_bps = 0;
    float samples_per_sec = 0;
  } rate;

  struct {
    int32_t min24 = INT_MAX;
    int32_t max24 = min24 * -1;
  } raw_val;

  struct {
    elapsedMillis window;
    float min = numerical_limits::max();
    float max = numerical_limits::min();
  } magnitude;

  struct {
    float freq_bin_width = 0.0;
  } config;

  struct {
    uint32_t fft_us[6] = {};
    float fft_avg_us = 0;

    uint32_t rx_us[6] = {};
    float rx_avg_us = 0;
  } durations;

  float bass_mag_floor = 0.0;
  size_t object_size = 0;
};

struct PinSpotStats {
  size_t object_size = 0;
};

struct LightDeskStats {
  using uint_limit = std::numeric_limits<uint_fast32_t>;
  const char *mode = nullptr;

  DmxStats_t dmx;
  FxStats_t fx;
  I2sStats_t i2s;
  PinSpotStats_t pinspot[2];

  struct {
    uint_fast32_t min = uint_limit::max();
    uint_fast32_t curr = 0.0;
    uint_fast32_t max = uint_limit::min();
  } frame_prepare;

  size_t object_size = 0;
};

const char *fxDesc(const Fx_t fx);
const char *modeDesc(const LightDeskMode_t mode);

} // namespace lightdesk
} // namespace ruth

#endif
