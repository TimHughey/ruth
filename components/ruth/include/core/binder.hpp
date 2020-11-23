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

#include "local/types.hpp"

namespace ruth {

typedef class Binder Binder_t;
typedef class TextBuffer<512> BinderRaw_t;
typedef class TextBuffer<10> RuntimeEnv_t;
typedef class TextBuffer<50> NtpServer_t;
typedef class TextBuffer<45> WifiConfigInfo_t;
typedef class TextBuffer<45> MqttConfigInfo_t;

class Binder {

public:
  Binder(){}; // SINGLETON
  static void init() { _inst_()->_init_(); }

  // Runtime environment
  static const char *env() { return _inst_()->env_.c_str(); };

  // MQTT
  static const char *mqttPasswd() { return _inst_()->mq_passwd_.c_str(); };
  static size_t mqttReconnectMs() { return _inst_()->mq_reconnect_ms_; }
  static size_t mqttRxBuffer() { return _inst_()->mq_rx_buffer_; }
  static size_t mqttTxBuffer() { return _inst_()->mq_tx_buffer_; }
  static uint32_t mqttTaskPriority() { return _inst_()->mq_task_prio_; };
  static const char *mqttUri() { return _inst_()->mq_uri_.c_str(); };
  static const char *mqttUser() { return _inst_()->mq_user_.c_str(); };

  // NTP
  static const char *ntpServer(int index);

  // WiFi
  static const char *wifiSsid() { return _inst_()->wifi_ssid_.c_str(); };
  static const char *wifiPasswd() { return _inst_()->wifi_passwd_.c_str(); };

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

  const char *config_path_ = "/ruthfs/config_0.json";

  BinderRaw_t raw_;
  size_t raw_size_ = 0;

  RuntimeEnv_t env_;
  time_t mtime_;
  bool active_;

  NtpServer_t ntp_servers_[2];
  WifiConfigInfo_t wifi_ssid_;
  WifiConfigInfo_t wifi_passwd_;

  MqttConfigInfo_t mq_uri_;
  MqttConfigInfo_t mq_user_;
  MqttConfigInfo_t mq_passwd_;
  uint32_t mq_task_prio_;
  size_t mq_rx_buffer_;
  size_t mq_tx_buffer_;
  uint32_t mq_reconnect_ms_;
};
} // namespace ruth

#endif
