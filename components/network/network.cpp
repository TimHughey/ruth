//  Ruth
//  Copyright (C) 2021 Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#include "network/network.hpp"
#include "ArduinoJson.h"

#include <cstring>
#include <esp_attr.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_sntp.h>
#include <iterator>
#include <lwip/apps/sntp.h>
#include <lwip/ip_addr.h>
#include <string_view>

using namespace std::literals;

namespace ruth {

// the sntp ready callback requires a static function so we need
// to keep a pointer to ourself at the class level.  this is definitely
// a hack however ESP-IDF will provide a better sntp API in a future release
Net *Net::self{nullptr};

Net::Net(Binder *binder) noexcept
    : netif{nullptr}, notify_task{xTaskGetCurrentTaskHandle()}, hostname(binder->hostname()),
      host_id(binder->host_id()) {

  if (self == nullptr) self = this; // for sntp callback (see note above)
  esp_err_t rc{ESP_OK};

  check_error(esp_event_loop_create_default(), "event loop");

  netif = esp_netif_create_default_wifi_sta();

  // set hostname BEFORE starting wifi
  rc = esp_netif_set_hostname(netif, hostname.c_str());
  check_error(rc, "set hostname");

  ESP_LOGI(TAG, "assigned name [%s]", hostname.c_str());

  wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  check_error(esp_wifi_init(&init_cfg), "init");

  rc = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &events, this);
  check_error(rc, "event handler");

  rc = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &events, this);
  check_error(rc, "ip event");

  rc = esp_wifi_set_storage(WIFI_STORAGE_FLASH);
  check_error(rc, "storage ");

  rc = esp_wifi_set_mode(WIFI_MODE_STA);
  check_error(rc, "set mode station");

  rc = esp_wifi_set_ps(WIFI_PS_NONE);
  check_error(rc, "set power save");

  auto proto = WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N;
  rc = esp_wifi_set_protocol(WIFI_IF_STA, proto);

  check_error(rc, "set protocol");

  wifi_config_t cfg{};
  cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  cfg.sta.bssid_set = 0;

  // get a reference from the binder for our config
  JsonObjectConst cfg_obj = binder->doc["network"].as<JsonObjectConst>();

  const auto ssid = cfg_obj["wifi"]["ssid"].as<const char *>();
  const auto pass = cfg_obj["wifi"]["passwd"].as<const char *>();

  memccpy(cfg.sta.ssid, ssid, 0x00, sizeof(cfg.sta.ssid) - 1);
  memccpy(cfg.sta.password, pass, 0x00, sizeof(cfg.sta.password) - 1);

  rc = esp_wifi_set_config(WIFI_IF_STA, &cfg);
  check_error(rc, "set config");

  esp_wifi_start();

  // capture remainder (required later) of binder config
  ip_timeout_ticks = pdMS_TO_TICKS(cfg_obj["timeout_ms"]["ip"] | 60000);
  sntp_timeout_ticks = pdMS_TO_TICKS(cfg_obj["timeout_ms"]["sntp"] | 60000);

  JsonArrayConst cfg_sntp_servers = cfg_obj["sntp_servers"].as<JsonArrayConst>();

  for (std::size_t i = 0; i < sntp_servers.size(); i++) {

    if (JsonVariantConst server = cfg_sntp_servers[i]; server) {
      const auto &s = sntp_servers[i].assign(server.as<string>());
      ESP_LOGI(TAG, "sntp server: %s", s.c_str());
    }
  }
}

void Net::acquired_ip(void *event_data) noexcept {
  [[maybe_unused]] const auto *got_ip{static_cast<ip_event_got_ip_t *>(event_data)};

  xTaskNotify(notify_task, Net::IP_READY, eSetBits);
}

void Net::acquired_time(struct timeval *) noexcept {

  // no further time sync notifications are required
  sntp_set_time_sync_notification_cb(nullptr);

  ESP_LOGI(TAG, "sntp initial sync complete");

  xTaskNotify(self->notify_task, Net::SNTP_READY, eSetBits);
  self = nullptr;
}

void Net::check_error(esp_err_t err, const auto *desc) noexcept {
  if (err == ESP_OK) return;

  ESP_LOGE(TAG, "widi %s %s", desc, esp_err_to_name(err));

  vTaskDelay(pdMS_TO_TICKS(5000)); // let things settle

  esp_restart();
}

void Net::events(void *ctx, esp_event_base_t base, int32_t id, void *data) noexcept {
  auto *net = static_cast<Net *>(ctx);

  if (ctx == nullptr) return;

  if (base == WIFI_EVENT) {
    if (id == WIFI_EVENT_STA_START) {
      esp_wifi_connect();
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
      esp_wifi_connect();
    }
  } else if ((base == IP_EVENT) && (id == IP_EVENT_STA_GOT_IP)) {
    net->acquired_ip(data);
  }
}

const std::string Net::ip4_address() noexcept {
  esp_netif_ip_info_t ip_info;
  std::array<char, IPADDR_STRLEN_MAX> ip_str{0x00};
  auto err = esp_netif_get_ip_info(netif, &ip_info);

  if (!err) {
    const esp_ip4_addr_t *ip = &(ip_info.ip);
    esp_ip4addr_ntoa(ip, ip_str.data(), ip_str.size());
  }

  return ip_str[0] ? std::string(ip_str.data()) : std::string("0.0.0.0");
}

void Net::sntp() noexcept {

  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  // sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
  sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

  for (std::size_t i = 0; i < sntp_servers.size(); i++) {
    ESP_LOGI(TAG, "sntp server[%u] %s", i, sntp_servers[i].c_str());
    sntp_setservername(i, sntp_servers[i].c_str());
  }

  sntp_set_time_sync_notification_cb(&acquired_time);

  // start up time sync
  sntp_init();
}

void Net::wait_for_ready() noexcept { // static
  uint32_t notify{0};

  ESP_LOGI(TAG, "standing by for IP address...");
  xTaskNotifyWait(0, IP_READY, &notify, ip_timeout_ticks);

  if (notify & IP_READY) {
    // we are connected and have an ip address, start sntp
    sntp();

    // now wait for initial time sync to complete
    ESP_LOGI(TAG, "standing by for sntp initial sync...");

    xTaskNotifyWait(0, SNTP_READY, &notify, sntp_timeout_ticks);

    if (notify ^ SNTP_READY) {
      ESP_LOGW(TAG, "timeout: initial time sync (%lu ticks)", sntp_timeout_ticks);
      esp_restart();
    }

  } else {
    ESP_LOGW(TAG, "timeout: ip address acquistion (%lu ticks)", ip_timeout_ticks);
    esp_restart();
  }
}

} // namespace ruth
