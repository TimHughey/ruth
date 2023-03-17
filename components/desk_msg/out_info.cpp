//  Ruth
//  Copyright (C) 2023  Tim Hughey
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

#include "desk_msg/out_info.hpp"

#include <array>
#include <esp_app_desc.h>
#include <esp_system.h>

namespace ruth {
namespace desk {

void MsgOutWithInfo::add_app_info(JsonDocument &doc) noexcept {
  std::array<char, 12> app_sha{0};

  esp_app_get_elf_sha256(app_sha.data(), app_sha.size());

  const auto app_desc = esp_app_get_description();

  JsonObject app = doc.createNestedObject("app");
  app["sha"] = app_sha.data();
  app["build_time"] = app_desc->time;
  app["build_date"] = app_desc->date;
  app["fw_vsn"] = app_desc->version;
  app["idf_vsn"] = app_desc->idf_ver;
  app["last_reset_reason"] = reset_reason().data();
}

void MsgOutWithInfo::add_heap_info(JsonDocument &doc) noexcept {

  JsonObject heap = doc.createNestedObject("heap");
  heap["min"] = esp_get_minimum_free_heap_size();
  heap["free"] = esp_get_free_heap_size();
  heap["max_alloc"] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  JsonObject task = doc.createNestedObject("task");
  task["count"] = uxTaskGetNumberOfTasks();
  task["stack_hw_mark"] = uxTaskGetStackHighWaterMark(nullptr);
}

csv MsgOutWithInfo::reset_reason() noexcept {
  switch (esp_reset_reason()) {

  case ESP_RST_POWERON:
    return csv{"power on"};

  case ESP_RST_EXT:
    return csv{"external pin"};

  case ESP_RST_SW:
    return csv{"esp_restart"};

  case ESP_RST_PANIC:
    return csv{"software panic"};

  case ESP_RST_INT_WDT:
    return csv{"interrupt watchdog"};

  case ESP_RST_TASK_WDT:
    return csv{"task watchdog"};

  case ESP_RST_WDT:
    return csv{"other watchdog"};

  case ESP_RST_DEEPSLEEP:
    return csv{"exit deep sleep"};

  case ESP_RST_BROWNOUT:
    return csv{"brownout"};

  case ESP_RST_SDIO:
    return csv{"SDIO"};

  case ESP_RST_UNKNOWN:
  default:
    return csv{"unknown"};
  }
}

} // namespace desk
} // namespace ruth