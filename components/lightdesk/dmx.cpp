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

static auto create_timer_args(auto callback, auto *obj, const char *name) noexcept {
  return esp_timer_create_args_t{callback, obj, ESP_TIMER_ISR, name, true /* skip missed */};
}

// object API

DMX::DMX(int64_t frame_us, Stats &&stats, size_t stack) noexcept //
    : frame_us(frame_us), uart_frame{0x00}, stats(std::move(stats)) {
  // note:  actual fdata will be moved into pending_fdata, zero fill not needed

  // wait for previous DMX task to stop, if there is one
  for (auto waiting_ms = 0; dmx_task; waiting_ms += 10) {
    vTaskDelay(pdMS_TO_TICKS(10));

    if ((waiting_ms % 100) == 0) {
      ESP_LOGW(DMX::TAG, "waiting to start task, %dms", waiting_ms);
    }
  }

  dmx::uart_init(UART_FRAME_LEN); // only inits uart once

  // it is essential we run at a higher priority to:
  //  -prevent data races on uart_frame
  //  -prevent flicker
  sender_task = xTaskGetCurrentTaskHandle();
  auto priority = uxTaskPriorityGet(sender_task) + 1;

  const auto sync_timer_args = create_timer_args(&frame_trigger, this, "dmx::spool");
  esp_timer_create(&sync_timer_args, &sync_timer);

  ESP_LOGI(TAG, "starting task priority=%d, sender task=%p ", priority, sender_task);

  xTaskCreatePinnedToCore(&kickstart, // run this function as the task
                          TAG,        // task name
                          stack,      // stack size
                          this,       // pass the newly created object to run()
                          priority,   // our priority
                          &dmx_task,  // task handle
                          1);
}

DMX::~DMX() noexcept {
  spooling = false;

  if (sync_timer) {
    if (esp_timer_is_active(sync_timer)) esp_timer_stop(sync_timer);
    esp_timer_delete(std::exchange(sync_timer, nullptr));
  }

  if (dmx_task) {
    TaskStatus_t info;

    vTaskGetInfo(dmx_task,  // task handle
                 &info,     // where to store info
                 pdTRUE,    // do not calculate task stack high water mark
                 eInvalid); // include task status

    if (info.eCurrentState != eSuspended) vTaskSuspend(dmx_task);

    ESP_LOGI(TAG, "task %s suspended, stack_hw_mark=%lu", info.pcTaskName,
             info.usStackHighWaterMark);

    vTaskDelete(std::exchange(dmx_task, nullptr));
  }
}

void IRAM_ATTR DMX::frame_trigger(void *dmx_v) noexcept {
  auto *self = static_cast<DMX *>(dmx_v);

  const auto nv = self->spooling ? Notifies::TRIGGER : Notifies::SHUTDOWN;
  if (self->dmx_task) xTaskNotifyFromISR(self->dmx_task, nv, eSetBits, nullptr);
}

void DMX::kickstart(void *dmx_v) noexcept {
  auto *self = static_cast<DMX *>(dmx_v);

  { // new scope so temporaries don't hang around on stack
    ESP_LOGI(TAG, "kickstart() in progress...");

    const float sync_ms = self->frame_us / 1000.0f;
    ESP_LOGI(TAG, "[kickstart] frame len=%d ms=%0.2f, uart_ms=%lld", UART_FRAME_LEN, sync_ms,
             FRAME_MS.count());
  }

  self->spooler();
}

void IRAM_ATTR DMX::spooler() noexcept {
  spooling = true;
  esp_timer_start_periodic(sync_timer, frame_us);

  // loop state
  uint32_t clear_in{TRIGGER};
  uint32_t clear_out{NONE};
  uint32_t nv{0};

  while (spooling) {

    xTaskNotifyWait(clear_in, clear_out, &nv, portMAX_DELAY);

    if (nv & SHUTDOWN) {
      spooling = false;
      if (sync_timer && esp_timer_is_active(sync_timer)) esp_timer_stop(sync_timer);
    };

    if (nv & TRIGGER) {
      // send the uart_frame (updated or not)
      auto bytes_tx = uart_write_bytes_with_break( // tx frame via uart
          dmx::UART_NUM,                           // uart_num
          uart_frame.data(),                       // data to send
          uart_frame.size(),                       // data length to send
          FRAME_BREAK.count());                    // duration of break

      if (bytes_tx == uart_frame.size()) {
        stats(Stats::FRAMES); // frame sent OK
      } else {
        ESP_LOGW(TAG, "bytes_tx=%d should be %u", bytes_tx, uart_frame.size());
      }

      stats(std::exchange(frame_pending, false) ? Stats::QOK : Stats::QRF);
    }
  }

  // we've fallen through the loop which means we're shutting down.
  // suspend ourself so the task can be safely deleted
  vTaskSuspend(nullptr);
}

} // namespace desk
} // namespace ruth