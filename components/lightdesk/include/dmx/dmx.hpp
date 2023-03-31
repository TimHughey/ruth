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

#include "io/io.hpp"
#include "ru_base/rut.hpp"
#include "ru_base/types.hpp"
#include "stats/stats.hpp"

#include <ArduinoJson.h>
#include <array>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <memory>

namespace ruth {

namespace desk {

class DMX {
public:
  using fdata_t = std::array<uint8_t, 25>;
  static auto constexpr TAG{"dmx"};

private:
  // break between frames, sent by UART, excluded from FRAME timing
  static constexpr Micros FRAME_BREAK{92us};

  // for documentation purposes we build up the DMX frame duration
  static constexpr Micros FRAME_MAB{12us};
  static constexpr Micros FRAME_BYTE{44us};
  static constexpr Micros FRAME_SC{FRAME_BYTE};
  static constexpr Micros FRAME_MTBF{44us};
  static constexpr Micros FRAME_DATA{FRAME_BYTE * 513};

  // frame interval does not include the BREAK as it is handled by the UART
  static constexpr Micros FRAME_US{FRAME_MAB + FRAME_SC + FRAME_DATA + FRAME_MTBF};
  static constexpr Millis FRAME_MS{FRAME_US.count() / 1000};

  // save the conversion from ms to ticks
  static constexpr TickType_t FRAME_TICKS{pdMS_TO_TICKS(FRAME_MS.count())};
  static constexpr TickType_t FRAME_TICKS25{FRAME_TICKS / 4};
  static constexpr TickType_t FRAME_TICKS10{FRAME_TICKS / 10};

  // length, in bytes, of the dmx frame to transmit
  static constexpr std::size_t UART_FRAME_LEN{450};

  enum Notifies : uint32_t {
    NONE = 0b00,
    WANT_FRAME = 0b1 << 0, // signal from DMX to caller
    HAVE_FRAME = 0b1 << 1,
    UART_FRAME_BUSY = 0b1 << 2,
    SENT_FRAME = 0b1 << 3,
    TRIGGER = 0b1 << 4,
    SHUTDOWN = 0b1 << 5
  };

public:
  DMX(int64_t frame_us, Stats &&stats, size_t stack = 3 * 1024) noexcept;
  ~DMX() noexcept;

  inline bool next_frame(fdata_t &fdata, std::ptrdiff_t bytes) noexcept {
    // note: the DMX task runs at a higher priority than the caller and is
    //       pinned to the same core.  to minimize data races updating the
    //       the uart frame we boost the caller's priority while it is
    //       writing to the uart frame.

    vTaskPrioritySet(nullptr, priority + 1);
    // copy the frame data into the uart frame
    std::copy(fdata.begin(), fdata.end(), uart_frame.begin());

    stats(std::exchange(frame_pending, true) ? Stats::QSF : Stats::QOK);

    vTaskPrioritySet(nullptr, priority - 1);

    return true;
  }

  inline void stats_calculate() noexcept { stats.calc(); }
  inline bool stats_pending() noexcept { return stats.pending(); }
  inline void stats_populate(JsonDocument &doc) noexcept { stats.populate(doc); }

  inline void track_data_wait(int64_t wait_us) noexcept { stats.track_data_wait(wait_us); }

private:
  static void frame_trigger(void *dmx_v) noexcept;
  static void kickstart(void *dmx_v) noexcept;

  void spooler() noexcept;

private:
  using uart_frame_t = std::array<uint8_t, UART_FRAME_LEN>;

private:
  // order dependent
  const int64_t frame_us;
  uart_frame_t uart_frame;
  Stats stats;
  TaskHandle_t sender_task;
  UBaseType_t priority;

  // order independent
  bool frame_pending{false};
  esp_timer_handle_t sync_timer{nullptr};
  bool spooling{false};

  static TaskHandle_t dmx_task;
};

} // namespace desk
} // namespace ruth