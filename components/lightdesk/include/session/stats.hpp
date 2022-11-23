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

#include <algorithm>
#include <atomic>
#include <esp_log.h>

namespace ruth {
namespace desk {

class stats {

public:
  stats(const Millis &i)
      : interval(i),    // how often fps is calculated
        fps{0.0},       // cached (last) calculated fps
        frame_count{0}, // count of frames for current internval
        mark{0}         // frame count at last fps calculation
  {}

  inline void calc() {
    // we can calculate fps when both mark and frame count are non-zero
    if (mark.load() && frame_count.load()) {
      // fps is calculated via the diff in frames since last calc
      // divided by the interval
      fps = (frame_count.load() - mark.load()) / interval.count();

      mark.store(frame_count.load()); // reference (mark) of frame count for the next calc
    } else if (frame_count.load()) {
      // fps hasn't been calculated yet, set mark to prepare for next calc
      mark.store(frame_count.load());
    }
  }

  inline float cached_fps() const noexcept { return fps; }
  inline void saw_frame() noexcept { frame_count++; }

private:
  // order dependent
  const Seconds interval;
  float fps;
  std::atomic_int64_t frame_count;
  std::atomic_int64_t mark;

public:
  static constexpr csv TAG{"SessionStats"};
};

} // namespace desk
} // namespace ruth