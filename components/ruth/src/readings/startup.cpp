/*
    startup_reading.cpp - Ruth Startup Reading
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
#include <ctime>

#include <esp_ota_ops.h>

#include "net/network.hpp"
#include "readings/startup.hpp"

namespace ruth {
namespace reading {
Startup::Startup(uint32_t batt_mv) : Remote(BOOT, batt_mv) {
  app_desc_ = esp_ota_get_app_description();

  reset_reason_ = decodeResetReason(esp_reset_reason());
};

void Startup::populateJSON(JsonDocument &doc) {
  char sha256[12] = {};

  Remote::populateJSON(doc);

  esp_ota_get_app_elf_sha256(sha256, sizeof(sha256));

  doc["app_elf_sha256"] = sha256;
  doc["build_time"] = app_desc_->time;
  doc["build_date"] = app_desc_->date;
  doc["firmware_vsn"] = app_desc_->version;
  doc["idf_vsn"] = app_desc_->idf_ver;
  doc["reset_reason"] = reset_reason_.c_str();
};

const string_t &Startup::decodeResetReason(esp_reset_reason_t reason) {
  static string_t _reason;

  switch (reason) {
  case ESP_RST_UNKNOWN:
    _reason = "unknown";
    break;

  case ESP_RST_POWERON:
    _reason = "power on";
    break;

  case ESP_RST_EXT:
    _reason = "external pin";
    break;
  case ESP_RST_SW:
    _reason = "esp_restart";
    break;

  case ESP_RST_PANIC:
    _reason = "software panic";
    break;

  case ESP_RST_INT_WDT:
    _reason = "interrupt watchdog";
    break;

  case ESP_RST_TASK_WDT:
    _reason = "task watchdog";
    break;

  case ESP_RST_WDT:
    _reason = "other watchdog";
    break;

  case ESP_RST_DEEPSLEEP:
    _reason = "exit deep sleep";
    break;

  case ESP_RST_BROWNOUT:
    _reason = "brownout";
    break;

  case ESP_RST_SDIO:
    _reason = "SDIO";
    break;

  default:
    _reason = "unknown";
  }

  return _reason;
}
} // namespace reading
} // namespace ruth
