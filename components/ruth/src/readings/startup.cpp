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
Startup::Startup() : Remote(BOOT) {
  app_desc_ = esp_ota_get_app_description();

  reset_reason_ = decodeResetReason(esp_reset_reason());
};

void Startup::populateMessage(JsonDocument &doc) {
  TextBuffer<12> sha256;

  Remote::populateMessage(doc);

  auto size = esp_ota_get_app_elf_sha256(sha256.data(), sha256.capacity());
  sha256.forceSize(size);

  doc["app_elf_sha256"] = sha256.c_str();
  doc["build_time"] = app_desc_->time;
  doc["build_date"] = app_desc_->date;
  doc["firmware_vsn"] = app_desc_->version;
  doc["idf_vsn"] = app_desc_->idf_ver;
  doc["reset_reason"] = reset_reason_;
};

const char *Startup::decodeResetReason(esp_reset_reason_t reason) {

  switch (reason) {
  case ESP_RST_UNKNOWN:
    return "unknown";

  case ESP_RST_POWERON:
    return "power on";

  case ESP_RST_EXT:
    return "external pin";

  case ESP_RST_SW:
    return "esp_restart";

  case ESP_RST_PANIC:
    return "software panic";

  case ESP_RST_INT_WDT:
    return "interrupt watchdog";

  case ESP_RST_TASK_WDT:
    return "task watchdog";

  case ESP_RST_WDT:
    return "other watchdog";

  case ESP_RST_DEEPSLEEP:
    return "exit deep sleep";

  case ESP_RST_BROWNOUT:
    return "brownout";

  case ESP_RST_SDIO:
    return "SDIO";

  default:
    return "unknown";
  }
}
} // namespace reading
} // namespace ruth
