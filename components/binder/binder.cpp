/*
    binder.cpp --  Abstraction for ESP32 SPIFFS
    Copyright (C) 2020  Tim Hughey

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

#include "binder/binder.hpp"
#include "ArduinoJson.h"

#include <time.h>

#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ruth {

static Binder_t binder;
static const char TAG[] = "Binder";

extern const uint8_t raw_start[] asm("_binary_binder_0_mp_start");
extern const size_t raw_bytes asm("binder_0_mp_length");

static constexpr size_t DOC_CAPACITY = 512;
StaticJsonDocument<DOC_CAPACITY> _embed_doc;

int64_t Binder::now() {
  struct timeval time_now;
  gettimeofday(&time_now, nullptr);

  auto us_since_epoch = time_now.tv_sec * 1000000L; // convert seconds to microseconds

  us_since_epoch += time_now.tv_usec; // add microseconds since last second

  return us_since_epoch;
}

void Binder::parse() {
  auto embed_err = deserializeMsgPack(_embed_doc, raw_start, raw_bytes);

  root = _embed_doc.as<JsonObject>();
  meta = root["meta"];

  const auto mtime = meta["mtime"].as<time_t>();

  constexpr size_t at_buff_len = 30;
  char binder_at[at_buff_len] = {};
  struct tm timeinfo = {};
  localtime_r(&mtime, &timeinfo);

  strftime(binder_at, at_buff_len, "%c", &timeinfo);

  ESP_LOGI(TAG, "%s doc_used[%d/%d]", binder_at, root.memoryUsage(), DOC_CAPACITY);

  if (embed_err) {
    ESP_LOGW(TAG, "parse failed %s, bytes[%d]", embed_err.c_str(), raw_bytes);
    vTaskDelay(portMAX_DELAY);
  }
}

// STATIC
Binder_t *Binder::i() { return &binder; }
} // namespace ruth
