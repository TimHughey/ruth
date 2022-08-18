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

#include <algorithm>
#include <esp_log.h>

#include "ru_base/time.hpp"
#include "ru_base/types.hpp"

namespace ruth {
namespace desk {

class stats {

public:
  stats(const Millis &i) : interval(ru_time::as_duration<Millis, Seconds>(i)) {}

  static constexpr csv TAG = "SessionStats";

  inline void calc() {
    if (mark && frame_count) {
      // enough info to calc fps
      fps = (frame_count - mark) / interval.count();

      if (fps < 42.5) {
        ESP_LOGI(TAG.data(), "fps=%2.2f", fps);
      }

      // save the current frame count as a reference (mark) for the next calc
      mark = frame_count;
    } else if (frame_count) {
      mark = frame_count;
      ESP_LOGI(TAG.data(), "set initial mark=%llu", mark);
    }
  }

  inline float cached_fps() const { return fps; }
  inline bool idle() const { return fps == 0.0; }
  inline void saw_frame() { frame_count++; }

private:
  const Seconds interval;
  TimePoint fps_start = steady_clock::now();
  uint64_t calcs = 0; // calcs * fps_start for "precise" timing
  float fps = 0;
  uint64_t frame_count = 0;
  uint64_t mark = 0;
};

} // namespace desk
} // namespace ruth