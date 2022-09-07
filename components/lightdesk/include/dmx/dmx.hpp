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

#include "dmx/frame.hpp"
#include "io/io.hpp"
#include "ru_base/time.hpp"
#include "ru_base/types.hpp"

#include <array>
#include <freertos/FreeRTOS.h>
#include <freertos/message_buffer.h>
#include <memory>

namespace ruth {

class DMX {
public:
  static constexpr csv TAG = "dmx";

private:
  static constexpr Micros FRAME_MAB = 12us;
  static constexpr Micros FRAME_BYTE = 44us;
  static constexpr Micros FRAME_SC = FRAME_BYTE;
  static constexpr Micros FRAME_MTBF = 44us;
  static constexpr Micros FRAME_DATA = Micros(FRAME_BYTE * 512);

  // frame interval does not include the BREAK as it is handled by the UART
  static constexpr Micros FRAME_US = FRAME_MAB + FRAME_SC + FRAME_DATA + FRAME_MTBF;
  static constexpr Millis FRAME_MS = ru_time::as_duration<Micros, Millis>(FRAME_US);
  static constexpr TickType_t FRAME_TICKS = pdMS_TO_TICKS(FRAME_MS.count());
  static constexpr TickType_t RECV_TIMEOUT = FRAME_TICKS * 2.5;
  static constexpr TickType_t QUEUE_TICKS = 1;

  static constexpr std::array<uint8_t, 3> SHUTDOWN{0xaa, 0xcc, 0x55};

private: // must use start to create object
  DMX() : live(true) {}

public:
  ~DMX();

  // returns raw pointer managed by unique_ptr
  static std::unique_ptr<DMX> init();

  bool tx_frame(dmx::frame &&frame);

  void stop() { live = false; }

private:
  void spool_frames();

private:
  // order independent
  bool live;
  MessageBufferHandle_t msg_buff;
  uint64_t queue_fail = 0;
  uint64_t timeouts = 0; // count of receive timeouts
};
} // namespace ruth