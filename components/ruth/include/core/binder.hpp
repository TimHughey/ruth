/*
    binder.hpp -- Abstraction for ESP32 SPIFFS
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

#ifndef _ruth_binder_hpp
#define _ruth_binder_hpp

#include <esp_vfs.h>
#include <esp_vfs_fat.h>

#include "external/ArduinoJson.h"
#include "local/types.hpp"

namespace ruth {

typedef class Binder Binder_t;

class Binder {

public:
  Binder(){}; // SINGLETON
  static void init() { _i_()->_init_(); }
  static Binder_t *instance() { return _i_(); }

  // Command Line Interface
  static bool cliEnabled() { return componentEnabled(BINDER_CLI); }

  static bool componentEnabled(BinderCategory_t category) {
    JsonObject component;

    switch (category) {
    case BINDER_CLI:
      component = _i_()->_cli;
      break;

    case BINDER_LIGHTDESK:
      component = _i_()->_lightdesk;
      break;

    default:
      break;
    }

    bool enabled = false;
    if (component) {
      enabled = component["enable"];
    }

    return enabled;
  }

  // cli interface
  const char *basePath() const { return base_path_; }
  size_t copyToFilesystem();
  int ls(const char *path = nullptr);
  size_t pretty(BinderPrettyJson_t &buff);
  int print();
  int rm(const char *path = nullptr);
  int versions();

  // Runtime environment
  static const char *env() { return _i_()->meta_["env"] | "prod"; }

  // LightDesk
  static bool lightDeskEnabled() { return componentEnabled(BINDER_LIGHTDESK); }

  // MQTT
  static const char *mqttPasswd() { return _i_()->mqtt_["passwd"]; };
  static size_t mqttReconnectMs() { return _i_()->mqtt_["reconnect_ms"]; }
  static size_t mqttRxBuffer() { return _i_()->mqtt_["rx_buffer"]; }
  static size_t mqttTxBuffer() { return _i_()->mqtt_["tx_buffer"]; }
  static uint32_t mqttTaskPriority() { return _i_()->mqtt_["task_priority"]; };
  static const char *mqttUri() { return _i_()->mqtt_["uri"]; }
  static const char *mqttUser() { return _i_()->mqtt_["user"]; }

  // NTP
  static const char *ntpServer(int index) { return _i_()->ntp_servers_[index]; }

  // OTA
  static const char *otaHost() {
    return _i_()->ota_["host"] | "www.example.com";
  }

  static const char *otaPath() { return _i_()->ota_["path"] | "nested/path"; }

  static const char *otaFile() { return _i_()->ota_["file"] | "latest.bin"; }

  // WiFi
  static const char *wifiSsid() { return _i_()->wifi_["ssid"]; }
  static const char *wifiPasswd() { return _i_()->wifi_["passwd"]; }

private:
  void _init_();
  static Binder *_i_();

  DeserializationError deserialize(JsonDocument &doc, BinderRaw_t &buff) const;
  void load();
  void parse();

private:
  const esp_vfs_fat_mount_config_t mount_config_ = {
      .format_if_mount_failed = true,
      .max_files = 4,
      .allocation_unit_size = CONFIG_WL_SECTOR_SIZE};

  wl_handle_t s_wl_handle_ = WL_INVALID_HANDLE;
  const char *base_path_ = "/r";

  const static size_t _doc_capacity =
      JSON_ARRAY_SIZE(2) + 2 * JSON_OBJECT_SIZE(1) + 2 * JSON_OBJECT_SIZE(2) +
      JSON_OBJECT_SIZE(3) + 2 * JSON_OBJECT_SIZE(7) + 32;

  static const uint8_t _embed_start_[] asm("_binary_binder_0_mp_start");
  static const uint8_t _embed_end_[] asm("_binary_binder_0_mp_end");
  static const size_t _embed_length_ asm("binder_0_mp_length");

  const char *binder_file_ = "/r/binder_0.mp";

  BinderRaw_t embed_raw_;
  BinderRaw_t file_raw_;

  StaticJsonDocument<_doc_capacity> embed_doc_;
  StaticJsonDocument<_doc_capacity> file_doc_;
  JsonObject _root;
  JsonObject _cli;
  JsonObject _lightdesk;
  JsonObject meta_;
  JsonObject mqtt_;
  JsonArray ntp_servers_;
  JsonObject ota_;
  JsonObject wifi_;

  time_t versions_[2] = {0, 0};
};
} // namespace ruth

#endif
