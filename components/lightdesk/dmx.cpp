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
#include "ru_base/rut.hpp"
#include "stats/stats.hpp"

#include <algorithm>
#include <esp_log.h>

namespace ruth {

namespace desk {

// object API

DMX::DMX(Stats &&stats, size_t stack) noexcept //
    : stats(std::move(stats)), uart_frame{0x00} {
  // note:  actual fdata will be moved into pending_fdata, zero fill not needed

  // wait for previous DMX task to stop, if there is one
  for (auto waiting_ms = 0; dmx_task; waiting_ms += 10) {
    vTaskDelay(pdMS_TO_TICKS(10));

    if ((waiting_ms % 100) == 0) {
      ESP_LOGW(DMX::TAG, "waiting to start task, %dms", waiting_ms);
    }
  }

  // note: ring buffer includes a header for each item
  // const auto buff_len = (frame_len + sizeof(std::size_t)) * 5;
  // mb = xMessageBufferCreate(buff_len);

  dmx::uart_init(UART_FRAME_LEN); // only inits uart once

  // it is essential we run at a higher priority to avoid lag
  // processing the ring buffer and the uart buffer so grab the
  // callers priority
  sender_task = xTaskGetCurrentTaskHandle();
  auto priority = uxTaskPriorityGet(sender_task) - 1;

  ESP_LOGI(TAG, "starting task priority=%d, sender task=%p ", priority, sender_task);

  xTaskCreate(&spool_frames, // run this function as the task
              TAG,           // task name
              stack,         // stack size
              this,          // pass the newly created object to run()
              priority,      // our priority
              &dmx_task);    // task handle
}

DMX::~DMX() noexcept {
  spooling = false;

  if (dmx_task) {
    TaskStatus_t info;

    do {
      vTaskGetInfo(dmx_task,  // task handle
                   &info,     // where to store info
                   pdTRUE,    // do not calculate task stack high water mark
                   eInvalid); // include task status

      if (info.eCurrentState != eSuspended) {
        vTaskDelay(FRAME_TICKS);

        ESP_LOGW(TAG, "task state=%u (want suspended(3))", info.eCurrentState);
      }
    } while (info.eCurrentState != eSuspended);

    ESP_LOGI(TAG, "task %s suspended, stack_hw_mark=%lu", info.pcTaskName,
             info.usStackHighWaterMark);

    vTaskDelete(std::exchange(dmx_task, nullptr));
    ESP_LOGI(TAG, "deleted dmx task=%p", dmx_task);
  }

  // // it is safe to delete the message buffer and the task
  // if (mb) vMessageBufferDelete(std::exchange(mb, nullptr));
}

void IRAM_ATTR DMX::spool_frames(void *dmx_v) noexcept { // static
  auto *self = static_cast<DMX *>(dmx_v);

  ESP_LOGI(TAG, "spool_frames() starting...");

  constexpr float tick_us{1000.0 / portTICK_PERIOD_MS};
  ESP_LOGI(TAG, "tick=%0.2fµs period_ms=%lu ", tick_us, portTICK_PERIOD_MS);
  ESP_LOGI(TAG, "self=%p task=%p", self, self->dmx_task);
  ESP_LOGI(TAG, "frame len=%d, timing ms=%lld ticks=%lu", UART_FRAME_LEN, FRAME_MS.count(),
           FRAME_TICKS);

  // everything is ready, start spooling
  self->spooling = true;

  auto last_wake = xTaskGetTickCount();
  xTaskNotify(self->sender_task, 0x1000, eSetBits); // tell sender we're ready

  while (self->spooling) {

    uint32_t nv{0};
    auto write_finished = xTaskNotifyWait(0x0000, 0x2000, &nv, 0);

    if ((write_finished == pdTRUE) && (nv == 0x2000)) {
      // tell sender we're copying the frame
      xTaskNotify(self->sender_task, 0x0000, eSetValueWithOverwrite);

      // frame has been deposited and is pending, copy it to uart_frame
      auto &fdata = self->pending_fdata.front();
      std::copy(fdata.begin(), fdata.end(), self->uart_frame.begin());

      self->stats(Stats::QOK);
    } else {
      // pending frame missed the train
      self->stats(Stats::QRF);
    }

    // tell sender we're ready for the next frame
    xTaskNotify(self->sender_task, 0x1000, eSetBits);

    // send the uart_frame (updated or not)
    auto bytes_tx = uart_write_bytes_with_break( // tx frame via uart
        dmx::UART_NUM,                           // uart_num
        self->uart_frame.data(),                 // data to send
        self->uart_frame.size(),                 // data length to send
        FRAME_BREAK.count());                    // duration of break

    if (bytes_tx == self->uart_frame.size()) {
      self->stats(Stats::FRAMES); // frame sent OK
    } else {
      ESP_LOGW(TAG, "bytes_tx=%d should be %u", bytes_tx, self->uart_frame.size());
    }

    // ensure proper frame timing
    xTaskDelayUntil(&last_wake, FRAME_TICKS);
  }

  ESP_LOGW(TAG, "dmx task suspending...");

  // we've fallen through the loop which means we're shutting down.
  // suspend ourself so the task can be safely deleted
  vTaskSuspend(nullptr);
}

} // namespace desk
} // namespace ruth