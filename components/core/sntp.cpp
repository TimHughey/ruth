/*
  Ruth
  (C)opyright 2021  Tim Hughey

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

#include <esp_log.h>
#include <esp_sntp.h>
#include <lwip/apps/sntp.h>
#include <lwip/err.h>
#include <lwip/sys.h>

#include "datetime.hpp"
#include "elapsed.hpp"

#include "sntp.hpp"

static const char *TAG = "Sntp";

namespace ruth {

Sntp::Sntp(const Opts &opts) : _opts(opts) {

  sntp_setoperatingmode(SNTP_OPMODE_POLL);

  sntp_setservername(0, opts.servers[0]);
  sntp_setservername(1, opts.servers[1]);
  sntp_init();

  _timer = xTimerCreate("sntp", pdMS_TO_TICKS(opts.check_ms), pdTRUE, (void *)this, &checkStatus);
  xTimerStart(_timer, 0);
}

Sntp::~Sntp() { xTimerDelete(_timer, pdMS_TO_TICKS(100)); }

void Sntp::checkStatus(TimerHandle_t handle) {
  Sntp *sntp = (Sntp *)pvTimerGetTimerID(handle);

  // NOTE
  // ensure nothing blocks -- this is a callback

  if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
    // notify the task waiting for SNTP to complete
    xTaskNotify(sntp->_opts.notify_task, sntp->_opts.notify_val, eSetValueWithOverwrite);

    // stop the timer from reloading
    xTimerStop(handle, 0);
  }
}

// void Sntp::ensureTimeIsSet() {
//   elapsedMillis sntp_elapsed;
//   const uint32_t wait_max_ms = 30000;
//   const uint32_t check_ms = 100;
//   auto wait_sntp = true;
//
//   while ((sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) && wait_sntp) {
//     // StatusLED::percent(0.05);
//     // delay(check_ms / 2);
//     // StatusLED::percent(0.50);
//
//     if (sntp_elapsed > wait_max_ms) {
//       wait_sntp = false;
//     } else {
//       // delay(check_ms / 2);
//     }
//   }
//
//   if (sntp_elapsed > wait_max_ms) {
//     ESP_LOGE(pcTaskGetTaskName(nullptr), "SNTP failed");
//     esp_restart();
//   } else {
//
//     ESP_LOGI(TAG, "SNTP complete, %s, elapsed[%0.1fs]", DateTime().c_str(), sntp_elapsed.toSeconds());
//   }
// }

} // namespace ruth
