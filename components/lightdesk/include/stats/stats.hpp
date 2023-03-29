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

#include "desk_msg/kv.hpp"
#include "ru_base/types.hpp"

#include <ArduinoJson.h>
#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <esp_log.h>
#include <type_traits>

namespace ruth {
namespace desk {

class Stats {

public:
  enum metric_t : uint8_t { DATA_WAIT = 0, FRAMES, MARK, FPS, QOK, QRF, QSF, END };

public:
  Stats(const std::uint32_t interval_ms) noexcept
      : interval(interval_ms / 1000), calculated{false} {}

  inline void operator()(metric_t m) noexcept {
    auto &metric = metrics[m];
    metric += 1;
  }

  inline void calc() noexcept {

    auto &frame_count = metrics[metric_t::FRAMES];
    auto &mark = metrics[metric_t::MARK]; // frame count at last calc

    // we can calculate fps when both mark and frame count are non-zero
    if (mark && frame_count) {
      // fps is calculated via the diff in frames since last calc
      // divided by the interval

      // ESP32 floating point is slow, multiply by 100 to capture the remainder
      // in the non-floating point type and the receiver will convert to float
      metrics[metric_t::FPS] = ((frame_count - mark) * 100) / interval;

      mark = frame_count; // reference (mark) of frame count for the next calc
      calculated = true;

    } else if (frame_count) {
      // fps hasn't been calculated yet, set mark to prepare for next calc
      mark = frame_count;
    }
  }

  inline bool pending() const noexcept { return calculated; }

  inline void populate(JsonDocument &doc) noexcept {
    doc[desk::SUPP] = true;

    doc[desk::DATA_WAIT_US] = metrics[metric_t::DATA_WAIT];
    doc[desk::FPS] = metrics[metric_t::FPS];
    doc[desk::QOK] = metrics[metric_t::QOK];
    doc[desk::QRF] = metrics[metric_t::QRF];
    doc[desk::QSF] = metrics[metric_t::QSF];

    calculated = false;

    // reset data wait tracking
    metrics[metric_t::DATA_WAIT] = 0;
  }

  inline void saw_frame() noexcept { ++metrics[metric_t::FRAMES]; }

  inline void track_data_wait(int64_t wait_us) noexcept {
    auto &max_wait = metrics[metric_t::DATA_WAIT];

    max_wait = std::max(max_wait, wait_us);
  }

private:
  // order dependent
  const uint32_t interval{2}; // frequency of calc, in seconds

  // order independent
  bool calculated{false};                           // stats calculated (ready for send)
  std::array<int64_t, metric_t::END> metrics{0x00}; // the metrics
};

} // namespace desk
} // namespace ruth