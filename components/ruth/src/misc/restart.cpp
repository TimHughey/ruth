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
static Restart_t *__singleton__ = nullptr;
static const char *NONE = "NONE";

Restart::Restart() {}

// STATIC
Restart_t *Restart::_instance_() {
  if (__singleton__ == nullptr) {
    __singleton__ = new Restart();
  }

  return __singleton__;
}

Restart::~Restart() {
  if (__singleton__) {
    delete __singleton__;
    __singleton__ = nullptr;
  }
}

void Restart::_now_() { _instance_()->restart(nullptr, nullptr, 0); }

void Restart::restart(const char *text, const char *func,
                      uint32_t reboot_delay_ms) {

  unique_ptr<char[]> func_buf(new char[256]);
  func_buf.get()[0] = 0x00;

  if (func) {
    snprintf(func_buf.get(), 255, " func=\"%s\"", func);
  }

  Text::rlog("restart, reason=\"%s\"%s", (text == nullptr) ? NONE : text,
             func_buf.get());

  // gracefully shutdown MQTT
  MQTT::shutdown();
  Net::stop();

  ESP_LOGW("Restart", "spooling ftl, jump in %dms...", reboot_delay_ms);
  vTaskDelay(pdMS_TO_TICKS(reboot_delay_ms));
  ESP_LOGW("Restart", "JUMP!");

  esp_restart();
}
} // namespace ruth
