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

#include "core/sntp.hpp"
#include "misc/status_led.hpp"

#include <esp_log.h>
#include <esp_sntp.h>
#include <lwip/apps/sntp.h>
#include <lwip/err.h>
#include <lwip/sys.h>

namespace ruth {

static Sntp *_instance_ = nullptr;

Sntp::Sntp(const Opts &opts) : _opts(opts) {

  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  // sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
  sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
  sntp_setservername(0, opts.servers[0]);
  sntp_setservername(1, opts.servers[1]);
  sntp_set_time_sync_notification_cb(Sntp::sync_callback);
  sntp_init();

  _instance_ = this;
}

Sntp::~Sntp() { _instance_ = nullptr; }

void Sntp::sync_callback(struct timeval *tv) {
  if (tv->tv_sec > 1624113088) {
    sntp_set_time_sync_notification_cb(nullptr);
    xTaskNotify(_instance_->_opts.notify_task, Sntp::READY, eSetValueWithOverwrite);
  }
}

} // namespace ruth
