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

#include <esp_spiffs.h>

#include "external/ArduinoJson.h"
#include "local/types.hpp"

namespace ruth {

typedef class Binder Binder_t;
typedef class TextBuffer<768> BinderRaw_t;
typedef class TextBuffer<10> RuntimeEnv_t;
typedef class TextBuffer<50> NtpServer_t;
typedef class TextBuffer<45> WifiConfigInfo_t;
typedef class TextBuffer<45> MqttConfigInfo_t;
typedef class TextBuffer<128> OtaHostname_t;
typedef class TextBuffer<128> OtaPath_t;
typedef class TextBuffer<25> OtaFile_t;

class Binder {

public:
  Binder(){}; // SINGLETON
  static void init() { _inst_()->_init_(); }

  // Command Line Interface
  static bool cliEnabled() { return _inst_()->doc_["cli"]["enable"]; }

  // Runtime environment
  static const char *env() { return _inst_()->doc_["env"] | "prod"; }

  // MQTT
  static const char *mqttPasswd() { return _inst_()->mqtt_["passwd"]; };
  static size_t mqttReconnectMs() { return _inst_()->mqtt_["reconnect_ms"]; }
  static size_t mqttRxBuffer() { return _inst_()->mqtt_["rx_buffer"]; }
  static size_t mqttTxBuffer() { return _inst_()->mqtt_["tx_buffer"]; }
  static uint32_t mqttTaskPriority() {
    return _inst_()->mqtt_["task_priority"];
  };
  static const char *mqttUri() { return _inst_()->mqtt_["uri"]; }
  static const char *mqttUser() { return _inst_()->mqtt_["user"]; }

  // NTP
  static const char *ntpServer(int index);

  // OTA
  static const char *otaHost() {
    return _inst_()->ota_["host"] | "www.example.com";
  }

  static const char *otaPath() {
    return _inst_()->ota_["path"] | "nested/path";
  }

  static const char *otaFile() { return _inst_()->ota_["file"] | "latest.bin"; }

  // WiFi
  static const char *wifiSsid() { return _inst_()->wifi_["ssid"]; }
  static const char *wifiPasswd() { return _inst_()->wifi_["passwd"]; }

private:
  void _init_();
  static Binder *_inst_();

  void load();
  void parse();

private:
  esp_vfs_spiffs_conf_t ruthfs_conf_ = {.base_path = "/ruthfs",
                                        .partition_label = "ruthfs",
                                        .max_files = 5,
                                        .format_if_mount_failed = false};

  const static size_t _doc_capacity =
      JSON_OBJECT_SIZE(1) + 3 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) +
      2 * JSON_OBJECT_SIZE(7) + 8;

  const char *config_path_ = "/ruthfs/config_0.json";

  BinderRaw_t raw_;
  size_t raw_size_ = 0;

  StaticJsonDocument<_doc_capacity> doc_;
  JsonObject mqtt_;
  JsonObject wifi_;
  JsonObject ntp_servers_;
  JsonObject ota_;
};
} // namespace ruth

#endif
