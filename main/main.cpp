//  Ruth
//  Copyright (C) 2017  Tim Hughey
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

#include "binder/binder.hpp"
#include "desk_cmd/ota.hpp"
#include "lightdesk/lightdesk.hpp"
#include "network/network.hpp"

#include <cstdlib>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

extern "C" {
void app_main(void);
}

// app_main() is intentionally kept to a minimum
// component/ruth contains full implementation

void app_main() {
  static const char *TAG{"app_main"};

  // prevent unnecessary logging by gpio API
  esp_log_level_set("gpio", ESP_LOG_ERROR);

  // since app_main() is the application entry point, we log something
  // so it's obvious where base ESP32 initialization code is complete and
  // our implementation begins
  constexpr float tick_us = 1000.0 / (float)portTICK_PERIOD_MS;

  ESP_LOGI(TAG, "portTICK_PERIOD_MS[%lu] tick[%0.2fµs]", portTICK_PERIOD_MS, tick_us);

  // Initialize NVS
  auto err = nvs_flash_init();

  if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
    ESP_LOGI(TAG, "nvs_flash_erase() required: %s", esp_err_to_name(err));

    ESP_ERROR_CHECK(nvs_flash_erase());

    err = nvs_flash_init();
    ESP_ERROR_CHECK(err);
  }

  ESP_LOGI(TAG, "nvs [%s]", esp_err_to_name(err));

  // initialize the binder
  auto binder = std::make_unique<ruth::Binder>();

  auto net = std::make_unique<ruth::Net>(binder.get());
  net->wait_for_ready();

  ruth::desk::OTA::validate_pending(binder.get());

  // this is where our implementation begins by starting the Core
  auto desk = std::make_unique<ruth::LightDesk>();

  desk->run(binder.get());

  esp_restart();
}
