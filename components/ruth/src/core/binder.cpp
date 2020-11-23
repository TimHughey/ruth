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

// inclusive of largest profile document
static const size_t _doc_capacity =
    3 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(7) + 236;

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
  if (_inst_()->ntp_servers_[index].size() > 0) {
    return _inst_()->ntp_servers_[index].c_str();
  } else {
    return nullptr;
  }
}

void Binder::parse() {
  StaticJsonDocument<_doc_capacity> doc;
  DeserializationError err = deserializeJson(doc, raw_.data(), raw_.size());

  if ((raw_size_ > 0) && !err) {
    // auto used = doc.memoryUsage();
    // auto used_percent = ((float)used / (float)_doc_capacity) * 100.0;
    // ESP_LOGI(TAG, "JsonDocument memory usage %0.1f%% (%u/%u)", used_percent,
    // used, _doc_capacity);

    env_ = doc["env"] | "prod";
    mtime_ = doc["meta"]["mtime"];
    active_ = doc["meta"]["active"];

    wifi_ssid_ = doc["wifi"]["ssid"] | "none";
    wifi_passwd_ = doc["wifi"]["passwd"] | "none";

    ntp_servers_[0] = doc["ntp"]["0"] | "pool.ntp.org";
    ntp_servers_[1] = doc["ntp"]["1"] | "pool.ntp.org";

    JsonObject mqtt = doc["mqtt"];
    mq_uri_ = mqtt["uri"] | "none";
    mq_user_ = mqtt["user"] | "none";
    mq_passwd_ = mqtt["passwd"] | "none";
    mq_task_prio_ = mqtt["task_priority"] | 11;
    mq_rx_buffer_ = mqtt["rx_buffer"] | 1024;
    mq_tx_buffer_ = mqtt["tx_buffer"] | 2048;
    mq_reconnect_ms_ = mqtt["reconnect_ms"] | 3000;

    ESP_LOGI(TAG, "contents dated %s", DateTime(mtime_).get());
  }
}

// STATIC
Binder_t *Binder::_inst_() { return &__singleton__; }

} // namespace ruth
