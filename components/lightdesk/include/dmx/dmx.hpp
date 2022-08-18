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
  static constexpr Micros FRAME_MAB = 12us;
  static constexpr Micros FRAME_BYTE = 44us;
  static constexpr Micros FRAME_SC = FRAME_BYTE;
  static constexpr Micros FRAME_MTBF = 44us;
  static constexpr Micros FRAME_DATA = Micros(FRAME_BYTE * 512);

  // frame interval does not include the BREAK as it is handled by the UART
  static constexpr Micros FRAME_US = FRAME_MAB + FRAME_SC + FRAME_DATA + FRAME_MTBF;
  static constexpr Millis FRAME_MS = ru_time::as_duration<Micros, Millis>(FRAME_US);

private: // must use start to create object
  DMX() : guard(io_ctx.get_executor()) {}

public:
  ~DMX();

  // returns raw pointer managed by unique_ptr
  static std::unique_ptr<DMX> init();

  void txFrame(DMX::Frame &&frame);

  void stop() {
    [[maybe_unused]] error_code ec;
    guard.reset();
  }

private:
  void renderLoop();

private:
  // order dependent
  io_context io_ctx;
  work_guard guard;

  MessageBufferHandle_t msg_buff;
};
} // namespace ruth