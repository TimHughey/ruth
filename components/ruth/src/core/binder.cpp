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
static Binder_t *__singleton__ = nullptr;
static const char TAG[] = "Binder";

// inclusive of largest profile document
static const size_t _doc_capacity =
    2 * JSON_ARRAY_SIZE(2) + 3 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) +
    JSON_OBJECT_SIZE(4) + 204;

Binder::Binder() {
  esp_err_t esp_rc = ESP_OK;

  // register ruthfs spiffs
  esp_rc = esp_vfs_spiffs_register(&ruthfs_conf_);
  // ESP_LOGI(TAG, "[%s] ruthfs registration", esp_err_to_name(esp_rc));

  if (esp_rc == ESP_OK) {
    size_t total = 0, used = 0;
    esp_rc = esp_spiffs_info(ruthfs_conf_.partition_label, &total, &used);

    // ESP_LOGI(TAG, "[%s] ruthfs partition total[%d] used[%d]",
    //          esp_err_to_name(esp_rc), total, used);

    load();
    parse();

    esp_vfs_spiffs_unregister(ruthfs_conf_.partition_label);

    // ESP_LOGI(TAG, "[%s] ruth unregister", esp_err_to_name(esp_rc));

    // struct stat st;
    // if (stat("/ruthfs/config_0.json", &st) == 0) {
    //   ESP_LOGI(TAG, "found /ruthfs/config_0.json mtime[%ld] size[%ld]",
    //            st.st_mtime, st.st_size);
    // }
    //
    // f = fopen("/ruthfs/config_ctrl.json", "w");
    // if (f == NULL) {
    //   ESP_LOGE(TAG, "unable to open /ruthfs/config_ctrl.json errno[%d]",
    //   errno);
    // } else {
    //   ESP_LOGI(TAG, "opened /ruthfs/config_ctrl.json for writing");
    // }
    // fclose(f);
    //
    // if (stat("/ruthfs/config_ctrl.json", &st) == 0) {
    //   ESP_LOGI(TAG, "found /ruthfs/config_ctrl.json mtime[%ld] size[%ld]",
    //            st.st_mtime, st.st_size);
    // }
  }
}

// STATIC
Binder_t *Binder::init() { return Binder::_instance_(); }

void Binder::load() {

  FILE *f = nullptr;

  f = fopen(config_path_, "r");

  if (f == nullptr) {
    ESP_LOGE(TAG, "%s open failed, errno[%d]", config_path_, errno);
    return;
  }

  raw_size_ = fread((void *)config_raw_, 1, 512, f);

  fclose(f);

  if (raw_size_ == 0) {
    ESP_LOGE(TAG, "%s read failed, errno[%d]", config_path_, errno);
  }
}

const char *Binder::ntpServer(int index) {
  if (_instance_()->ntp_servers_[index].size() > 0) {
    return _instance_()->ntp_servers_[index].c_str();
  } else {
    return nullptr;
  }
}

void Binder::parse() {
  StaticJsonDocument<_doc_capacity> doc;
  DeserializationError err = deserializeJson(doc, config_raw_);

  if (raw_size_ > 0) {
    ESP_LOGI(TAG, "%s raw bytes[%d]", config_path_, raw_size_);
  }

  if (err) {
    ESP_LOGE(TAG, "%s deserialization failed %s", config_path_, err.c_str());
    return;
  }

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
  mqtt_uri_ = mqtt["uri"] | "none";
  mqtt_user_ = mqtt["user"] | "none";
  mqtt_passwd_ = mqtt["passwd"] | "none";

  ESP_LOGI(TAG, "contents dated %s", DateTime(mtime_).get());
}

// STATIC
Binder_t *Binder::_instance_() {
  if (__singleton__ == nullptr) {
    __singleton__ = new Binder();
  }
  return __singleton__;
}

Binder::~Binder() {
  if (__singleton__) {

    delete __singleton__;
    __singleton__ = nullptr;
  }
}

// void Binder::publishMsg(const char *key, BinderMessage_t *blob) {
//   unique_ptr<struct tm> timeinfo(new struct tm);
//
//   localtime_r(&(blob->time), timeinfo.get());
//
//   Text::rlog(timeinfo.get(), "key(%s) msg(%s)", key, blob->msg);
// }

} // namespace ruth
