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

#include "dmx/dmx.hpp"
#include "dmx/uart.hpp"
#include "io/io.hpp"
#include "ru_base/rut.hpp"

#include <algorithm>
#include <array>
#include <esp_log.h>

namespace ruth {

static DRAM_ATTR TaskHandle_t dmx_task{nullptr};

static void ensure_one_task() noexcept {
  for (auto waiting_ms = 0; dmx_task; waiting_ms++) {
    vTaskDelay(pdMS_TO_TICKS(1));

    if ((waiting_ms % 100) == 0) {
      ESP_LOGW(DMX::TAG.data(), "waiting to start task, %dms", waiting_ms);
    }
  }
}

// object API

DMX::DMX(std::size_t frame_len) noexcept : frame_len{frame_len}, spooling{false} {
  ESP_LOGI(TAG.data(), "startup, frame=%lldms (%lu ticks), len=%u", FRAME_MS.count(), FRAME_TICKS,
           frame_len);

  // wait for previous DMX task to stop, if there is one
  ensure_one_task();

  // note: ring buffer includes a header for each item
  rb = xRingbufferCreateNoSplit(frame_len, 5);

  dmx::uart_init(UART_FRAME_LEN); // only inits uart once

  // get the priority of the task that is starting us
  // it is essential that we run at a lower priority than the caller
  // feeding the ring buffer otherwise we could be woken up by
  // incomplete ring buffer actions via tx_frame()
  auto caller_priority = uxTaskPriorityGet(nullptr);

  auto rc = xTaskCreate(&spool_frames,       // run this function as the task
                        TAG.data(),          // task name
                        3 * 1024,            // stack size
                        this,                // pass the newly created object to run()
                        caller_priority - 1, // always lower priority than caller
                        &dmx_task);

  ESP_LOGI(TAG.data(), "started rc=%d task=%p", rc, dmx_task);
}

DMX::~DMX() noexcept {
  spooling = false;

  TaskStatus_t info;

  do {
    vTaskGetInfo(dmx_task,  // task handle
                 &info,     // where to store info
                 pdFALSE,   // do not calculate task stack high water mark
                 eInvalid); // include task status

    if (info.eCurrentState != eSuspended) vTaskDelay(pdMS_TO_TICKS(1));
  } while (info.eCurrentState != eSuspended);

  // it is safe to delete the ring buffer and the task
  vRingbufferDelete(std::exchange(rb, nullptr));
  vTaskDelete(std::exchange(dmx_task, nullptr));
}

void IRAM_ATTR DMX::spool_frames(void *dmx_v) noexcept { // static
  auto *self = static_cast<DMX *>(dmx_v);

  ESP_LOGI(TAG.data(), "spool frame loop starting...");

  // declare the uart frame
  std::array<uint8_t, UART_FRAME_LEN> uart_frame{0x00};

  // declare the control variables used in the spool loop
  std::size_t bytes_recv;
  std::size_t bytes_tx;
  void *fdata_v;
  uint8_t *fdata;

  // everything this ready, start spooling
  self->spooling = true;

  while (self->spooling) {

    // get the frame data that was sent for rendering, prepare to send via uart
    // notes:
    //  1. sent frame data may not be the full frame sent to the uart
    //  2. the frame sent to the uart must be at least UART_FRAME_LEN to prevent flicker
    //     on headunits that go dark when frame data is not received
    bytes_recv = 0;
    fdata_v = xRingbufferReceive(self->rb, &bytes_recv, RECV_TIMEOUT_TICKS);

    // first, confirm we received data from the ring buffer
    if (fdata_v == nullptr) {
      // failed to recv bytes due to timeout, track this metric
      self->qrf++;

    } else [[likely]] {
      fdata = static_cast<uint8_t *>(fdata_v);

      // merge the received frame data into the uart frame
      // NOTE: we allow zero len frame data (aka silent frame)
      std::size_t i;
      for (i = 0; i < self->frame_len; i++) {
        // note: zero out uart frame when not enough bytes were received
        uart_frame[i] = (i < bytes_recv) ? *fdata++ : 0x00;
      }

      // done with the frame from the ringbuffer, return it
      vRingbufferReturnItem(self->rb, std::exchange(fdata_v, nullptr));

      // ensure the uart tx is complete before attempting to receive next frame
      // we do this check here to utilize the time between receiving frames
      // and allow the uart to send the data and break (low for 88µs)
      if (ESP_OK != uart_wait_tx_done(dmx::UART_NUM, FRAME_TICKS)) {
        self->uart_tx_fail++;
      }

      bytes_tx = uart_write_bytes_with_break( // tx frame via uart
          dmx::UART_NUM,                      // uart_num
          uart_frame.data(),                  // data to send
          uart_frame.size(),                  // number of bytes
          dmx::FRAME_BREAK);                  // bits to send as frame break

      if (bytes_tx == uart_frame.size()) self->qok++; // frame recv'ed OK
    }
  }

  // we've fallen through the loop which means we're shutting down.
  // suspend ourself so the task can be safely deleted
  vTaskSuspend(nullptr);
}

bool IRAM_ATTR DMX::tx_frame(JsonArrayConst &&fdata) noexcept {
  if (!spooling) return false; // not running, don't queue a frame

  auto rc = pdTRUE; // assume the best

  void *fdata_v{nullptr};

  if (rc = xRingbufferSendAcquire(rb, &fdata_v, frame_len, FRAME_TICKS25); rc == pdTRUE) {
    uint8_t *rb_data = static_cast<uint8_t *>(fdata_v);

    // copy the frame buffer array (which could be empty but never more than frame_len)
    for (const auto v : fdata) {
      *rb_data++ = v.as<uint8_t>();
    }

    // indicate we are finished with the send buffer and track metrics
    if (rc = xRingbufferSendComplete(rb, fdata_v); rc == pdTRUE) qok++;

  } else {
    qsf++; // ring buffer timeout
  }

  return (rc == pdTRUE) ? true : false;
}

} // namespace ruth