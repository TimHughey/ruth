/*
    Ruth
    Copyright (C) 2022  Tim Hughey

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

#pragma once

#include "ru_base/time.hpp"
#include "ru_base/types.hpp"

#include <esp_log.h>

namespace ruth {
namespace desk {

struct stats {
  TimePoint fps_start = steady_clock::now();
  uint64_t calcs = 0; // calcs * fps_start for "precise" timing
  float fps = 0;
  uint64_t frame_count = 0;
  uint64_t frame_shorts = 0;
  uint64_t mark = 0;

  static constexpr Seconds FRAME_STATS_SECS = 2s;
  static constexpr csv TAG = "desk::stats";

  inline void calc() {
    if (mark && frame_count) {
      // enough info to calc fps
      fps = (frame_count - mark) / FRAME_STATS_SECS.count();

      if (fps < 43.0) {
        ESP_LOGI(TAG.data(), "fps=%2.2f", fps);
      }

      // save the current frame count as a reference (mark) for the next calc
      mark = frame_count;
    }
  }

  inline void saw_frame() { frame_count++; }
  inline void saw_short_frame() { frame_shorts++; }

  inline float fps_now() const { return fps; }
  inline bool idle() const { return fps == 0.0; }
  inline void reset() { *this = stats(); }
};

} // namespace desk
} // namespace ruth