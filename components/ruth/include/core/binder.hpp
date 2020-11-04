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

#include <vector>

#include <esp_spiffs.h>

#include "local/types.hpp"

namespace ruth {

using std::vector;

typedef class Binder Binder_t;

class Binder {
public:
  static const char *env() { return _instance_()->env_.c_str(); };
  static Binder_t *init();
  static const char *mqttPasswd() {
    return _instance_()->mqtt_passwd_.c_str();
  };
  static const char *mqttUri() { return _instance_()->mqtt_uri_.c_str(); };
  static const char *mqttUser() { return _instance_()->mqtt_user_.c_str(); };
  static const char *ntpServer(int index);
  static const char *wifiSsid() { return _instance_()->wifi_ssid_.c_str(); };
  static const char *wifiPasswd() {
    return _instance_()->wifi_passwd_.c_str();
  };

private:
  Binder();
  ~Binder();
  static Binder *_instance_();

  void load();
  void parse();

private:
  esp_vfs_spiffs_conf_t ruthfs_conf_ = {.base_path = "/ruthfs",
                                        .partition_label = "ruthfs",
                                        .max_files = 5,
                                        .format_if_mount_failed = false};

  const char *config_path_ = "/ruthfs/config_0.json";

  char config_raw_[512];
  size_t raw_size_ = 0;

  string_t env_;
  time_t mtime_;
  bool active_;

  string_t ntp_servers_[2];
  string_t wifi_ssid_;
  string_t wifi_passwd_;

  string_t mqtt_uri_;
  string_t mqtt_user_;
  string_t mqtt_passwd_;
};
} // namespace ruth

#endif
