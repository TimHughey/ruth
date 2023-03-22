//  Ruth
//  Copyright (C) 2020  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "desk_msg/kv_store.hpp"
#include "io/io.hpp"
#include "ru_base/rut.hpp"
#include "ru_base/types.hpp"

#include <ArduinoJson.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <memory>

namespace ruth {

class DMX {
public:
  static auto constexpr TAG{"dmx"};

private:
  // for documentation purposes we build up the DMX frame duration
  static constexpr Micros FRAME_MAB{12us};
  static constexpr Micros FRAME_BYTE{44us};
  static constexpr Micros FRAME_SC{FRAME_BYTE};
  static constexpr Micros FRAME_MTBF{44us};
  static constexpr Micros FRAME_DATA{Micros(FRAME_BYTE * 512)};

  // frame interval does not include the BREAK as it is handled by the UART
  static constexpr Micros FRAME_US{FRAME_MAB + FRAME_SC + FRAME_DATA + FRAME_MTBF};
  static constexpr Millis FRAME_MS{rut::as<Millis, Micros>(FRAME_US)};

  // save the conversion from ms to ticks
  static constexpr TickType_t FRAME_TICKS{pdMS_TO_TICKS(FRAME_MS.count())};
  static constexpr TickType_t FRAME_HALF_TICKS{FRAME_TICKS / 2};
  static constexpr TickType_t FRAME_TICKS25{FRAME_TICKS / 4};
  static constexpr TickType_t RECV_TIMEOUT_TICKS{FRAME_TICKS * 2};

  // length, in bytes, of the dmx frame to transmit
  static constexpr std::size_t UART_FRAME_LEN{412};

public:
  DMX(std::size_t frame_len) noexcept;
  ~DMX() noexcept;

  // queue statistics, qok + qrf + qsf = total frames
  inline auto q_ok() const noexcept { return qok; } // queue ok count
  inline auto q_rf() const noexcept { return qrf; } // queue recv failures
  inline auto q_sf() const noexcept { return qsf; } // queue send failurs

  inline void populate_stats(desk::kv_store &kvs) noexcept {
    // queue statistics, qok + qrf + qsf = total frames
    kvs.add(desk::QOK, qok);
    kvs.add(desk::QRF, qrf);
    kvs.add(desk::QSF, qsf);
    kvs.add(desk::UART_OVERRUN, uart_tx_fail);
  }

  bool tx_frame(JsonArrayConst &&fdata) noexcept;

private:
  static void spool_frames(void *dmx_v) noexcept;

private:
  // order dependent
  const std::size_t frame_len; // frame_len to queue
  bool spooling;

  // order indepdent
  std::int32_t qok{0};          // count of frames queued/dequeued ok
  std::int32_t qrf{0};          // count of frame dequeue failures
  std::int32_t qsf{0};          // count of frame queue failures
  std::int32_t uart_tx_fail{0}; // count of uart overruns (timeouts)

  // order independent
  RingbufHandle_t rb;
};
} // namespace ruth