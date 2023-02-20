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

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <esp_attr.h>
#include <esp_log.h>
#include <iterator>
#include <source_location>
#include <string_view>

using namespace std::literals;

namespace ruth {

namespace shared {
std::unique_ptr<Net> net;
}

extern "C" {
int setenv(const char *envname, const char *envval, int overwrite);
void tzset(void);
}

Net::Net(Net::Opts &&opts) noexcept //
    : opts(std::move(opts)),        //
      netif{nullptr},               //
      mac_address{13, 0x00},        // mac_address created first
      host_id("ruth.")              // initialize prefix
{
  esp_err_t rc{ESP_OK};

  esp_netif_init();

  check_error(esp_event_loop_create_default());

  netif = esp_netif_create_default_wifi_sta();

  wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  check_error(esp_wifi_init(&init_cfg));

  rc = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &events, this);
  check_error(rc);

  rc = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &events, this);
  check_error(rc);

  rc = esp_wifi_set_storage(WIFI_STORAGE_FLASH);
  check_error(rc);

  rc = esp_wifi_set_mode(WIFI_MODE_STA);
  check_error(rc);

  rc = esp_wifi_set_ps(WIFI_PS_NONE);
  check_error(rc);

  rc = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G);

  check_error(rc);

  wifi_config_t cfg{};
  cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  cfg.sta.bssid_set = 0;

  // memcpy(cfg.sta.ssid, opts.ssid, sizeof(cfg.sta.ssid));
  // memcpy(cfg.sta.password, opts.passwd, sizeof(cfg.sta.password));

  memccpy(cfg.sta.ssid, opts.ssid, 0x00, sizeof(cfg.sta.ssid) - 1);
  memccpy(cfg.sta.password, opts.passwd, 0x00, sizeof(cfg.sta.password) - 1);

  rc = esp_wifi_set_config(WIFI_IF_STA, &cfg);
  check_error(rc);

  esp_wifi_start();

  ESP_LOGI(TAG, "standing by for IP address...");

  if (xTaskGetCurrentTaskHandle() == opts.notify_task) {
    uint32_t notify;

    xTaskNotifyWait(0, READY, &notify, pdMS_TO_TICKS(opts.ip_max_wait));

    if (!(notify & READY)) {
      ESP_LOGW(TAG, "ip address aquisition timeout [%lums]", opts.ip_max_wait);
      esp_restart();
    }
  }

  // create text representation of mac address
  uint8_t bytes[6];
  esp_wifi_get_mac(WIFI_IF_STA, bytes);

  char *p = mac_address.data();
  for (auto i = 0; i < sizeof(bytes); i++) {
    // this is knownly duplicated code to avoid creating dependencies
    if (bytes[i] < 0x10) *p++ = '0';       // zero pad when less than 0x10
    itoa(bytes[i], p, 16);                 // convert to hex
    p = (bytes[i] < 0x10) ? p + 1 : p + 2; // move pointer forward based on zero padding
  }

  // create host id
  static constexpr csv id_prefix{"ruth."};
  csv wrapped_mac_addr{mac_address.data()};

  host_id.append(mac_address.data());

  // initial hostname is the host id
  hostname = host_id;
}

void Net::acquired_ip(void *event_data) noexcept {
  xTaskNotify(opts.notify_task, Net::READY, eSetBits);
}

void Net::check_error(esp_err_t err, const src_loc loc) {
  if (err == ESP_OK) return;

  const size_t max_msg_len = 256;
  char *msg = new char[max_msg_len];

  vTaskDelay(pdMS_TO_TICKS(1000)); // let things settle

  snprintf(msg, max_msg_len, "[%s] %s", esp_err_to_name(err), loc.function_name());
  ESP_LOGE(TAG, "%s", msg);

  esp_restart();
}

void Net::events(void *ctx, esp_event_base_t base, int32_t id, void *data) noexcept {

  auto *net = static_cast<Net *>(ctx);

  if (ctx == nullptr) return;

  if (base == WIFI_EVENT) {
    if (id == WIFI_EVENT_STA_START) {
      esp_netif_set_hostname(net->netif, net->host_id.data());
      esp_wifi_connect();
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
      esp_wifi_connect();
    }
  } else if ((base == IP_EVENT) && (id == IP_EVENT_STA_GOT_IP)) {
    net->acquired_ip(data);
  }
}

const char *Net::name(const char *name) noexcept {

  // set the host's name if passed a valid pointer
  if (name != nullptr) {
    csv name_wrapped{name};

    hostname.assign(name_wrapped.begin(), name_wrapped.end());

    // memccpy(hostname.data(), name, 0x00, hostname.size());

    ESP_LOGI(TAG, "assigned name [%s]", hostname.data());

    esp_netif_set_hostname(netif, hostname.data());
  }

  return hostname.data();
}

} // namespace ruth
