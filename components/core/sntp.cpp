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

#include "sntp.hpp"
#include "status_led.hpp"

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

  auto &status_led = sntp->_status_led;
  if (status_led) {
    StatusLED::brighter();
    status_led = false;
  } else {
    StatusLED::dimmer();
    status_led = true;
  }

  // NOTE
  // ensure nothing blocks -- this is a callback

  if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
    // notify the task waiting for SNTP to complete
    xTaskNotify(sntp->_opts.notify_task, Sntp::READY, eSetValueWithOverwrite);
    StatusLED::percent(0.05);

    // stop the timer from reloading
    xTimerStop(handle, 0);
  }
}

} // namespace ruth
