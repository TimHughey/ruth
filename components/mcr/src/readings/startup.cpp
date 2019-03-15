/*
    celsius.cpp - Master Control Remote Celsius Reading
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

#include <esp_log.h>
#include <external/ArduinoJson.h>

#include "readings/startup_reading.hpp"

startupReading::startupReading(time_t mtime) : Reading(mtime) {
  reset_reason_m = decodeResetReason(esp_reset_reason());

  ESP_LOGI("mcrStartup", "reason: %s", reset_reason_m.c_str());
};

void startupReading::populateJSON(JsonObject &root) {
  root["type"] = "boot";
  root["hw"] = "esp32";
  root["reset_reason"] = reset_reason_m;
};

const std::string &
startupReading::decodeResetReason(esp_reset_reason_t reason) {
  static std::string _reason;

  switch (reason) {
  case ESP_RST_UNKNOWN:
    _reason = "undetermined";
    break;

  case ESP_RST_POWERON:
    _reason = "power on";
    break;

  case ESP_RST_EXT:
    _reason = "external pin";
    break;
  case ESP_RST_SW:
    _reason = "esp_restart()";
    break;

  case ESP_RST_PANIC:
    _reason = "sofware panic";
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

  default:
    _reason = "undefined";
  }

  _reason.append(" reset");

  return _reason;
}
