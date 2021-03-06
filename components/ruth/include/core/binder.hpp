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
  typedef TextBuffer<512> Raw;
  typedef TextBuffer<768> PrettyJson;

public:
  Binder(){}; // SINGLETON
  static void init() { i()->_init_(); }
  static Binder_t *instance() { return i(); }

  // Command Line Interface
  static bool cliEnabled() { return componentEnabled(BINDER_CLI); }

  static bool componentEnabled(BinderCategory_t category) {
    JsonObject component;

    switch (category) {
    case BINDER_CLI:
      component = i()->_cli;
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
  const char *basePath() const { return _base_path; }
  size_t copyToFilesystem();
  int ls(const char *path = nullptr);
  size_t pretty(PrettyJson &buff);
  int print();
  int rm(const char *path = nullptr);
  int versions();

  // Runtime environment
  static const char *env() { return i()->_meta["env"] | "prod"; }

  // LIghtDesk and DMX
  static uint16_t dmxMagic() { return i()->_dmx["magic"] | 0xc9d2; }
  static uint dmxPort() { return i()->_dmx["port"] | 48005; }
  static const char *dmxPsk() { return i()->_dmx["psk"] | "psk"; }
  static uint dmxVersion() { return i()->_dmx["version"] | 1; }
  static bool lightDeskEnabled() { return i()->_lightdesk["enable"] | false; }

  // MQTT
  static const char *mqttPasswd() { return i()->_mqtt["passwd"]; };
  static size_t mqttReconnectMs() { return i()->_mqtt["reconnect_ms"]; }
  static size_t mqttRxBuffer() { return i()->_mqtt["rx_buffer"]; }
  static size_t mqttTxBuffer() { return i()->_mqtt["tx_buffer"]; }
  static uint32_t mqttTaskPriority() { return i()->_mqtt["task_priority"]; };
  static const char *mqttUri() { return i()->_mqtt["uri"]; }
  static const char *mqttUser() { return i()->_mqtt["user"]; }

  // NTP
  static const char *ntpServer(int index) { return i()->_ntp_servers[index]; }

  // OTA
  static const char *otaHost() { return i()->_ota["host"] | "www.example.com"; }

  static const char *otaPath() { return i()->_ota["path"] | "nested/path"; }
  static const char *otaFile() { return i()->_ota["file"] | "latest.bin"; }
  static uint32_t otaValidMs() { return i()->_ota["valid_ms"] | 60000; }

  // WiFi
  static const char *wifiSsid() { return i()->_wifi["ssid"]; }
  static const char *wifiPasswd() { return i()->_wifi["passwd"]; }

private:
  void _init_();
  static Binder *i();

  DeserializationError deserialize(JsonDocument &doc, Raw &buff) const;
  void load();
  void parse();

private:
  const esp_vfs_fat_mount_config_t _mount_config = {
      .format_if_mount_failed = true,
      .max_files = 4,
      .allocation_unit_size = CONFIG_WL_SECTOR_SIZE};

  wl_handle_t _s_wl_handle = WL_INVALID_HANDLE;
  const char *_base_path = "/r";

  static const size_t _doc_capacity = 612;
  static const uint8_t _embed_start_[] asm("_binary_binder_0_mp_start");
  static const uint8_t _embed_end_[] asm("_binary_binder_0_mp_end");
  static const size_t _embed_length_ asm("binder_0_mp_length");

  const char *_binder_file = "/r/binder_0.mp";

  Raw _embed_raw;
  Raw _file_raw;

  StaticJsonDocument<_doc_capacity> _embed_doc;
  StaticJsonDocument<_doc_capacity> _file_doc;
  JsonObject _root;
  JsonObject _cli;
  JsonObject _dmx;
  JsonObject _lightdesk;
  JsonObject _meta;
  JsonObject _mqtt;
  JsonArray _ntp_servers;
  JsonObject _ota;
  JsonObject _wifi;

  time_t _versions[2] = {0, 0};
};
} // namespace ruth

#endif
