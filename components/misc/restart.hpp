/*
    restart.hpp -- Abstraction for esp_restart()
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

#ifndef _ruth_restart_hpp
#define _ruth_restart_hpp

#include <stdarg.h>
#include <sys/time.h>
#include <time.h>

#include <esp_system.h>

#include "network.hpp"
#include "protocols/dmx.hpp"
#include "protocols/mqtt.hpp"
#include "readings/text.hpp"

namespace ruth {

typedef class Restart Restart_t;

class Restart {

  using TR = reading::Text;

public:
  Restart(const char *format, ...) {
    va_list arglist;
    RestartMsg_t msg;

    va_start(arglist, format);
    msg.printf(format, arglist);
    va_end(arglist);

    restartActual(msg.c_str(), nullptr, 0);
  }

  Restart(const char *text = nullptr, const char *func = nullptr, uint32_t reboot_delay_ms = 0) {
    restartActual(text, func, reboot_delay_ms);
  }

  bool now() { return true; }

private:
  void restartActual(const char *text, const char *func, uint32_t reboot_delay_ms) {

    if (text && func) {
      TR::rlog("\"%s\" %s", text, func);
    } else if (text) {
      TR::rlog("%s", text);
    }

    // gracefully shutdown protocols and network
    MQTT::shutdown();
    Net::stop();

    ESP_LOGW("Restart", "spooling ftl, jump in %dms...", reboot_delay_ms);
    vTaskDelay(pdMS_TO_TICKS(reboot_delay_ms));
    ESP_LOGW("Restart", "JUMP!");

    esp_restart();
  }
};

} // namespace ruth

#endif // restart.hpp
