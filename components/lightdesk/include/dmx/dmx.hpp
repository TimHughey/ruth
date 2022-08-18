/*
    Ruth
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

#include "io/io.hpp"
#include "msg.hpp"
#include "ru_base/time.hpp"
#include "ru_base/types.hpp"

#include <array>
#include <freertos/message_buffer.h>
#include <memory>

namespace ruth {

namespace dmx {
constexpr size_t FRAME_LEN = 384;
using frame_data = std::array<uint8_t, FRAME_LEN>;
} // namespace dmx

class DMX {
public:
  static constexpr csv TAG = "dmx";

  class Frame : public dmx::frame_data {
  public:
    static constexpr size_t FRAME_LEN = 384; // minimum to prevent flicker

  public:
    inline Frame() : dmx::frame_data{0} {}

    Frame(Frame &src) = delete;                  // no copy assignment
    Frame(const Frame &src) = delete;            // no copy constructor
    Frame &operator=(Frame &rhs) = delete;       // no copy assignment
    Frame &operator=(const Frame &rhs) = delete; // no copy assignment
    Frame(Frame &&src) = default;                // allow move assignment
    Frame &operator=(Frame &&rhs) = default;     // allow copy assignment
  };

private:
  struct Stats {
    TimePoint fps_start = steady_clock::now();
    uint64_t calcs = 0; // calcs * fps_start for "precise" timing
    float fps = 0;
    uint64_t frame_count = 0;
    uint64_t frame_shorts = 0;
    uint64_t mark = 0;
  };

private: // must use start to create object
  DMX() : fps_timer(io_ctx) {}

public:
  ~DMX();

  // returns raw pointer managed by unique_ptr
  static std::unique_ptr<DMX> init();

  float fps() const { return stats.fps; }
  float idle() const { return stats.fps == 0.0f; }

  void txFrame(DMX::Frame &&frame);

  void stop() {
    [[maybe_unused]] error_code ec;
    fps_timer.cancel(ec);
  }

private:
  void fpsCalculate();
  void renderLoop();

private:
  // order dependent
  io_context io_ctx;
  steady_timer fps_timer;

  MessageBufferHandle_t msg_buff;

  Stats stats;
};
} // namespace ruth