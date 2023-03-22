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

#include "ru_base/rut.hpp"
#include "ru_base/types.hpp"

#include <algorithm>
#include <atomic>
#include <esp_log.h>

namespace ruth {
namespace desk {

class Stats {

public:
  Stats(const int64_t &interval_ms) noexcept
      : interval(interval_ms / 1000), // how often fps is calculated
        fps{0.0f},                    // cached (last) calculated fps
        frame_count{0},               // count of frames for current interval
        mark{0}                       // frame count at last fps calculation
  {}

  inline void calc() noexcept {
    // we can calculate fps when both mark and frame count are non-zero
    if (mark && frame_count.load()) {
      // fps is calculated via the diff in frames since last calc
      // divided by the interval
      fps = (frame_count.load() - mark.load()) / interval.count();

      mark.store(frame_count.load()); // reference (mark) of frame count for the next calc
    } else if (frame_count.load()) {
      // fps hasn't been calculated yet, set mark to prepare for next calc
      mark.store(frame_count.load());
    }
  }

  inline int64_t cached_fps() const noexcept { return fps; }
  inline void saw_frame() noexcept { frame_count++; }

private:
  // order dependent
  const Seconds interval;
  float fps;
  std::atomic_int32_t frame_count;
  std::atomic_int32_t mark;
};

} // namespace desk
} // namespace ruth