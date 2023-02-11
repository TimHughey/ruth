//  Ruth
//  Copyright (C) 2021  Tim Hughey
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

#include "core/startup_msg.hpp"

#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

namespace ruth {
namespace message {

Startup::Startup() {
  _filter.addLevel("host");
  _filter.addLevel("startup");
}

void Startup::assembleData(JsonObject &data) {
  constexpr size_t app_sha_length = 12;
  char app_sha[app_sha_length + 1] = {};

  esp_app_get_elf_sha256(app_sha, app_sha_length);

  const auto app_desc = esp_app_get_description();

  data["app_sha"] = app_sha;
  data["build_time"] = app_desc->time;
  data["build_date"] = app_desc->date;
  data["firmware_vsn"] = app_desc->version;
  data["idf_vsn"] = app_desc->idf_ver;
  data["reset_reason"] = resetReason();
}

const char *Startup::resetReason() {
  auto reason = esp_reset_reason();

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
} // namespace message
} // namespace ruth
