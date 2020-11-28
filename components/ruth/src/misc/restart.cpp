/*
    restart.cpp - Abstraction for esp_restart()
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

#include <sys/time.h>
#include <time.h>

#include "misc/restart.hpp"
#include "net/network.hpp"
#include "protocols/mqtt.hpp"

namespace ruth {

Restart::Restart(const char *text, const char *func, uint32_t reboot_delay_ms) {
  if (text && func) {
    Text::rlog("restart, \"%s\" %s", text, func);
  } else if (text) {
    Text::rlog("restart, \"%s\"", text);
  }

  // gracefully shutdown MQTT
  MQTT::shutdown();
  Net::stop();

  ESP_LOGW("Restart", "spooling ftl, jump in %dms...", reboot_delay_ms);
  vTaskDelay(pdMS_TO_TICKS(reboot_delay_ms));
  ESP_LOGW("Restart", "JUMP!");

  esp_restart();
}

bool Restart::now() { return true; }
} // namespace ruth
