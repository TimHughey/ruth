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
  static constexpr Micros FRAME_DATA{FRAME_BYTE * 512};

  // frame interval does not include the BREAK as it is handled by the UART
  static constexpr Micros FRAME_US{FRAME_MAB + FRAME_SC + FRAME_DATA + FRAME_MTBF};
  static constexpr Millis FRAME_MS{FRAME_US.count() / 1000};

  // save the conversion from ms to ticks
  static constexpr TickType_t FRAME_TICKS{pdMS_TO_TICKS(FRAME_MS.count())};
  static constexpr TickType_t FRAME_TICKS25{FRAME_TICKS / 4};
  static constexpr TickType_t FRAME_TICKS10{FRAME_TICKS / 10};

  // length, in bytes, of the dmx frame to transmit
  static constexpr std::size_t UART_FRAME_LEN{421};

public:
  DMX(Stats &&stats, size_t stack = 3 * 1024) noexcept;
  ~DMX() noexcept;

  inline bool next_frame(fdata_t &&fdata, std::ptrdiff_t bytes) noexcept {
    uint32_t nv{0};
    auto rc = xTaskNotifyWait(0x0000, 0x1000, &nv, FRAME_TICKS10);

    if ((rc == pdTRUE) && (nv == 0x1000)) {
      // we can store the pending fdata
      std::swap(pending_fdata.front(), fdata);

      // note: notification index 1 is to signal fdata write complete
      xTaskNotify(dmx_task, 0x2000, eSetBits);
      return true;
    }

    // task hasn't picked up the previous notification, drop fdata
    stats(Stats::QSF);
    return false;
  }

  inline void stats_calculate() noexcept { stats.calc(); }
  inline bool stats_pending() noexcept { return stats.pending(); }
  inline void stats_populate(JsonDocument &doc) noexcept { stats.populate(doc); }

  inline void track_data_wait(int64_t wait_us) noexcept { stats.track_data_wait(wait_us); }

private:
  static void spool_frames(void *dmx_v) noexcept;

private:
  using uart_frame_t = std::array<uint8_t, UART_FRAME_LEN>;
  static constexpr size_t pending_max{2};

private:
  // order dependent
  Stats stats;
  uart_frame_t uart_frame;
  std::array<fdata_t, pending_max> pending_fdata;
  uint8_t pat{0}; // pending allocation table

  // order independent
  bool spooling{false};
  TaskHandle_t sender_task{nullptr};
  TaskHandle_t dmx_task{nullptr};
};

} // namespace desk
} // namespace ruth