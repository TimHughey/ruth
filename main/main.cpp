/*
    main.cpp - Ruth Remote Main App
    Copyright (C) 2017  Tim Hughey

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

#include <cstdlib>

#include <esp_log.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/core.hpp"

using namespace ruth;

extern "C" {
void app_main(void);
int setenv(const char *envname, const char *envval, int overwrite);
void tzset(void);
}

// app_main() contains minimal implementation to keep code base maintainable
// for actual implementation see component/ruth

void app_main() {

  // prevent unnecessary logging by gpio API
  esp_log_level_set("gpio", ESP_LOG_ERROR);

  // since app_main() is the application entry point, we log something
  // so it's obvious where base ESP32 initialization code is complete and
  // our implementation begins
  float tick_us = 1000.0 / (float)portTICK_PERIOD_MS;

  ESP_LOGI("Rcore", "portTICK_PERIOD_MS[%u] tick[%0.2fµs]", portTICK_PERIOD_MS,
           tick_us);

  // set timezone to Eastern Standard Time
  // for now we set the timezone here in app_main() since it's foundational
  setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
  tzset();

  // this is where our implementation begins by starting the Core
  TaskHandle_t app_task = xTaskGetCurrentTaskHandle();
  Core::start(app_task);

  esp_task_wdt_reset();

  for (;;) {
    Core::loop();
  }
}
