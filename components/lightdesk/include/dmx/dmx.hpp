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

#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/message_buffer.h>
#include <future>
#include <memory>
#include <optional>

namespace ruth {

class DMX {
public:
  static constexpr csv TAG{"dmx"};

private:
  static constexpr Micros FRAME_MAB{12us};
  static constexpr Micros FRAME_BYTE{44us};
  static constexpr Micros FRAME_SC{FRAME_BYTE};
  static constexpr Micros FRAME_MTBF{44us};
  static constexpr Micros FRAME_DATA{Micros(FRAME_BYTE * 512)};

  // frame interval does not include the BREAK as it is handled by the UART
  static constexpr Micros FRAME_US{FRAME_MAB + FRAME_SC + FRAME_DATA + FRAME_MTBF};
  static constexpr Millis FRAME_MS{ru_time::as_duration<Micros, Millis>(FRAME_US)};
  static constexpr TickType_t FRAME_TICKS{pdMS_TO_TICKS(FRAME_MS.count())};
  static constexpr TickType_t RECV_TIMEOUT{FRAME_TICKS * 3};
  static constexpr TickType_t QUEUE_TICKS{1};

private: // must use start to create object
  DMX() noexcept : qok{0}, qrf(0), qsf(0) {}

public:
  ~DMX() noexcept;

  // returns raw pointer managed by unique_ptr
  static std::unique_ptr<DMX> init();

  // queue statistics, qok + qrf + qsf = total frames
  inline auto q_ok() const noexcept { return qok.load(); } // queue ok count
  inline auto q_rf() const noexcept { return qrf.load(); } // queue recv failures
  inline auto q_sf() const noexcept { return qsf.load(); } // queue send failurs

  std::future<bool> stop() noexcept { return shutdown_prom.emplace().get_future(); }

  void spool_frames() noexcept;
  bool tx_frame(dmx::frame &&frame);

private:
  // order dependent
  std::atomic_int64_t qok; // count of frames queued/dequeued ok
  std::atomic_int64_t qrf; // count of frame dequeue failures
  std::atomic_int64_t qsf; // count of frame queue failures

  // order independent
  std::optional<std::promise<bool>> shutdown_prom;
  MessageBufferHandle_t msg_buff;
};
} // namespace ruth