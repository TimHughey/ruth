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

#include <cstdlib>

#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <esp_log.h>

#include <esp_system.h>
#include <freertos/FreeRTOS.h>

#include "core/binder.hpp"
#include "external/ArduinoJson.h"
#include "misc/datetime.hpp"

namespace ruth {
static Binder_t __singleton__;
static const char TAG[] = "Binder";

void Binder::_init_() {
  esp_err_t esp_rc = ESP_OK;

  // register ruthfs spiffs
  esp_rc = esp_vfs_spiffs_register(&ruthfs_conf_);

  if (esp_rc == ESP_OK) {
    size_t total = 0, used = 0;
    esp_rc = esp_spiffs_info(ruthfs_conf_.partition_label, &total, &used);

    load();
    parse();

    esp_vfs_spiffs_unregister(ruthfs_conf_.partition_label);
  }
}

void Binder::load() {

  FILE *f = nullptr;

  if ((f = fopen(config_path_, "r")) != nullptr) {
    raw_size_ = fread(raw_.data(), 1, raw_.capacity(), f);
    raw_.forceSize(raw_size_);

    fclose(f);
  }
}

const char *Binder::ntpServer(int index) {
  switch (index) {
  case 0:
    return _inst_()->ntp_servers_["0"];
  case 1:
    return _inst_()->ntp_servers_["1"];
  default:
    return (const char *)"pool.ntp.org";
  }
}

void Binder::parse() {
  DeserializationError err = deserializeJson(doc_, raw_.data(), raw_.size());

  if ((raw_size_ > 0) && !err) {
    // auto used = doc.memoryUsage();
    // auto used_percent = ((float)used / (float)_doc_capacity) * 100.0;
    // ESP_LOGI(TAG, "JsonDocument memory usage %0.1f%% (%u/%u)", used_percent,
    // used, _doc_capacity);
    wifi_ = doc_["wifi"];
    ntp_servers_ = doc_["ntp"];
    mqtt_ = doc_["mqtt"];
    ota_ = doc_["ota"];

    float used_percent =
        ((float)doc_.memoryUsage() / (float)_doc_capacity) * 100.0;
    ESP_LOGI(TAG, "%s doc_used[%2.1f%%] (%d/%d)",
             DateTime(doc_["meta"]["mtime"].as<uint32_t>()).c_str(),
             used_percent, doc_.memoryUsage(), _doc_capacity);
  }
}

// STATIC
Binder_t *Binder::_inst_() { return &__singleton__; }

} // namespace ruth
