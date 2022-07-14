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

#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "io/io.hpp"
#include "msg.hpp"
#include "state/state.hpp"

#include "esp_pthread.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <memory>
#include <pthread.h>
#include <vector>

namespace ruth {

class DMX;
typedef std::shared_ptr<DMX> shDMX;

namespace shared {
static shDMX __dmx;
} // namespace shared

namespace dmx {

constexpr size_t FRAME_LEN = 384;
constexpr size_t TX_LEN = FRAME_LEN < 128 ? 0 : FRAME_LEN + 1;
class Frame : public uint8v {
public:
  Frame() : uint8v(dmx::FRAME_LEN) {}

  Frame(Frame &src) = delete;                  // no copy assignment
  Frame(const Frame &src) = delete;            // no copy constructor
  Frame &operator=(Frame &rhs) = delete;       // no copy assignment
  Frame &operator=(const Frame &rhs) = delete; // no copy assignment

  Frame(Frame &&src) = default;
  Frame &operator=(Frame &&rhs) = default;

  void preventFlicker() {
    // ensure the frame is of sufficient bytes to prevent flickering
    if (auto need_bytes = dmx::TX_LEN - size(); need_bytes > 0) {
      for (auto i = 0; i < need_bytes; i++) {
        emplace_back(0x00);
      }
    }
  }
};

} // namespace dmx

class DMX : public std::enable_shared_from_this<DMX> {

private:
  struct Stats {
    float fps = 0;
    uint64_t frame_count = 0;
    uint64_t frame_shorts = 0;
    uint64_t mark = 0;
  };

private:
  DMX(); // all access via shared_ptr

public:
  static shDMX create() {
    shared::__dmx = shDMX(new DMX());
    return ptr();
  }

  inline static shDMX ptr() { return shared::__dmx->shared_from_this(); }
  static void reset() { shared::__dmx.reset(); }

  ~DMX() = default;

  float framesPerSecond() const { return stats.fps; }
  static void handleFrame(dmx::Frame &&frame) { ptr()->txFrame(std::forward<dmx::Frame>(frame)); }

  float idle() const { return stats.fps == 0.0f; }

  void txFrame(dmx::Frame &&frame); // move frame to here

  static void start();
  void stop() {
    asio::post(io_ctx, [self = ptr()]() {
      self->io_ctx.stop();
      self->state.zombie();
    });
  }

private:
  void fpsCalculate();

  void renderLoop();
  static void *run(void *data);

  esp_err_t uartInit();

private:
  // order dependent
  io_context io_ctx;
  steady_timer fps_timer;

  // order independent
  desk::State state;

  Stats stats;
  static constexpr csv TAG = "DMX";
};
} // namespace ruth