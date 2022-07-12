/*
    protocols/dmx.hpp - Ruth DMX Protocol Engine
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

#pragma once

#include "base/ru_time.hpp"
#include "base/uint8v.hpp"
#include "io/io.hpp"
#include "msg.hpp"
#include "state/state.hpp"

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <set>
#include <string>
#include <vector>

namespace ruth {

class DMX;
typedef std::unique_ptr<DMX> upDMX;

class DMX {

public:
  typedef uint8v Frame;

private:
  // FRAME constants
  static constexpr size_t FRAME_LEN = 384;
  static constexpr size_t TX_LEN = FRAME_LEN < 128 ? 0 : FRAME_LEN + 1;
  static constexpr size_t RX_MAX_LEN = 512;

public:
  struct Stats {
    float fps = 0;
    uint64_t frame_count = 0;
    uint64_t frame_shorts = 0;
  };

public:
  DMX(io_context &io_ctx);
  ~DMX() = default;

  // uint64_t frameInterval() const { return _frame_us; }
  // float frameIntervalAsSeconds() {
  //   const float frame_us = static_cast<float>(_frame_us);
  //   const float frame_secs = (frame_us / (1000.0f * 1000.0f));
  //   return frame_secs;
  // }

  float framesPerSecond() const { return stats.fps; }

  float idle() const { return stats.fps == 0.0f; }

  void render() {
    state = State::RENDER;
    render_start = ru_time::nowNanos();
    renderLoop();
  }

  void stop();

private:
  void fpsCalculate();

  void renderLoop();
  void txFrame(const dmx::Packet &packet);
  esp_err_t uartInit();

private:
  // order dependent
  strand local_strand;

  steady_timer fps_timer;

  // order independent
  State state = State::IDLE;
  DMX::Frame frame; // the DMX frame starts as all zeros
  TimePoint render_start;

  uint64_t frame_count_mark = 0;
  float fpcp = 0; // frames per calculate period

  Stats stats;
};
} // namespace ruth