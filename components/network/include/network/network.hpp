/*
     protocols/dmx.cpp - Ruth Dmx Protocol Engine
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

#pragma once

#include <cstdlib>
#include <esp_event.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ruth {

typedef class Net Net_t;
class Net {
public:
  struct Opts {
    const char *ssid = nullptr;
    const char *passwd = nullptr;
    TaskHandle_t notify_task = nullptr;
  };

public:
  enum Notifies : uint32_t { READY = 0x01 };

public:
  Net() = default; // SINGLETON
  static const char *ca_start() { return (const char *)_ca_start_; };
  static const uint8_t *ca_end() { return _ca_end_; };
  static const char *disconnectReason(wifi_err_reason_t reason);
  static const char *hostID();
  static bool hostIdAndNameAreEqual();
  static const char *hostname();
  static const char *macAddress();
  static void setName(const char *name);
  static bool start(const Opts &opts);
  static void stop();

private:
  void acquiredIP(void *event_data);
  static void checkError(const char *func, esp_err_t err);
  void connected(void *event_data);
  void disconnected(void *event_data);
  void init();
  static void ip_events(void *ctx, esp_event_base_t base, int32_t id, void *data);
  static void wifi_events(void *ctx, esp_event_base_t base, int32_t id, void *data);

private:
  Opts _opts;
  esp_err_t init_rc_ = ESP_FAIL;
  esp_netif_t *netif_ = nullptr;

  static constexpr size_t mac_addr_max_len = 13;
  char _mac_addr[mac_addr_max_len] = {};

  static constexpr size_t max_name_and_id_len = 32;
  char _host_id[max_name_and_id_len] = {};
  char _hostname[max_name_and_id_len] = {};
  bool reconnect = true;

  static const uint8_t _ca_start_[] asm("_binary_ca_pem_start");
  static const uint8_t _ca_end_[] asm("_binary_ca_pem_end");
};

} // namespace ruth
