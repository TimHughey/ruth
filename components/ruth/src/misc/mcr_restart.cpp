/*
    mcr_restart.cpp - MCR abstraction for esp_restart()
    Copyright (C) 2019  Tim Hughey

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

#include <string>

#include <sys/time.h>
#include <time.h>

#include "misc/restart.hpp"
#include "net/network.hpp"
#include "protocols/mqtt.hpp"

namespace ruth {
static Restart_t *__singleton__ = nullptr;

Restart::Restart() {}

// STATIC
Restart_t *Restart::instance() {
  if (__singleton__) {
    return __singleton__;

  } else {

    __singleton__ = new Restart();
    return __singleton__;
  }
}

Restart::~Restart() {
  if (__singleton__) {
    delete __singleton__;
    __singleton__ = nullptr;
  }
}

// STATIC
void Restart::now() { instance()->restart(nullptr, nullptr, 0); }

void Restart::restart(const char *text, const char *func,
                         uint32_t reboot_delay_ms) {

  ESP_LOGW("Restart", "%s requested restart [%s]",
           (func == nullptr) ? "<UNKNOWN FUNCTION>" : func,
           (text == nullptr) ? "UNSPECIFIED REASON" : text);

  if (text) {
    textReading_t *rlog = new textReading(text);
    textReading_ptr_t rlog_ptr(rlog);

    MQTT::instance()->publish(rlog);

    // pause to ensure reading has been published
    // FUTURE:  query MQTT to ensure all messages have been sent
    //          rather than wait a hardcoded duration
    vTaskDelay(pdMS_TO_TICKS(1500));
  }

  Net::deinit();

  ESP_LOGW("Restart", "spooling ftl for jump in %dms...", reboot_delay_ms);
  vTaskDelay(pdMS_TO_TICKS(reboot_delay_ms));
  ESP_LOGW("Restart", "JUMP!");

  esp_restart();
}
} // namespace ruth
