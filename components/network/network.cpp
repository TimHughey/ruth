/*
     Ruth
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

#include "network/network.hpp"

#include <cstdlib>
#include <cstring>
#include <esp_attr.h>
#include <esp_log.h>
#include <string_view>

using namespace std::literals;

namespace ruth {

extern "C" {
int setenv(const char *envname, const char *envval, int overwrite);
void tzset(void);
}

static const char *base = "ruth.";
static const char base_len = 5;
static const char *TAG = "Net";
static Net_t __singleton__;

void Net::acquiredIP(void *event_data) { xTaskNotify(_opts.notify_task, Net::READY, eSetBits); }

void Net::checkError(const char *func, esp_err_t err) {
  if (err == ESP_OK) {
    return;
  }

  const size_t max_msg_len = 256;
  char *msg = new char[max_msg_len];

  vTaskDelay(pdMS_TO_TICKS(1000)); // let things settle

  snprintf(msg, max_msg_len, "[%s] %s", esp_err_to_name(err), func);
  ESP_LOGE(TAG, "%s", msg);

  esp_restart();
}

void Net::connected(void *event_data) {}

void Net::disconnected(void *event_data) {

  if (reconnect_) {
    esp_wifi_connect();
  }
}

const char *Net::disconnectReason(wifi_err_reason_t reason) {

  switch (reason) {
  case 1:
    return (const char *)"unspecified";
  case 2:
    return (const char *)"auth expire";
  case 3:
    return (const char *)"auth leave";
  case 4:
    return (const char *)"auth expire";
  case 5:
    return (const char *)"assoc too many";
  case 6:
    return (const char *)"not authed";
  case 7:
    return (const char *)"not associated";
  case 8:
    return (const char *)"assoc leave";

  default:
    return nullptr;
  }
}

const char *Net::hostID() {
  auto &host_id = __singleton__._host_id;

  if (host_id[0] == 0x00) {
    char *next = (char *)memccpy(host_id, base, 0x00, max_name_and_id_len);
    // memccpy returns a pointer to the byte immediately following the copied null
    memccpy(next - 1, macAddress(), 0x00, max_name_and_id_len - base_len);
  }

  return host_id;
}

bool Net::hostIdAndNameAreEqual() {
  auto &host_id = __singleton__._host_id;
  auto &hostname = __singleton__._hostname;

  return strcmp(host_id, hostname) == 0;
}

const char *Net::hostname() { return __singleton__._hostname; }

void Net::init() {
  esp_err_t rc = ESP_OK;

  esp_netif_init();

  rc = esp_event_loop_create_default();
  checkError(__PRETTY_FUNCTION__, rc); // never returns if rc != ESP_OK
  netif_ = esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  rc = esp_wifi_init(&cfg);
  checkError(__PRETTY_FUNCTION__, rc);

  rc = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_events, this);
  checkError(__PRETTY_FUNCTION__, rc);

  rc = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_events, this);
  checkError(__PRETTY_FUNCTION__, rc);

  rc = esp_wifi_set_storage(WIFI_STORAGE_FLASH);
  checkError(__PRETTY_FUNCTION__, rc);

  init_rc_ = rc;
}

void Net::ip_events(void *ctx, esp_event_base_t base, int32_t id, void *data) {

  Net_t *net = (Net_t *)ctx;

  if (ctx == nullptr) {
    return;
  }

  switch (id) {
  case IP_EVENT_STA_GOT_IP:
    net->acquiredIP(data);
    break;

  default:
    break;
  }
}

const char *Net::macAddress() {
  auto *mac_addr = __singleton__._mac_addr;

  if (mac_addr[0] != 0x00)
    return mac_addr;

  // assemble it

  constexpr size_t num_bytes = 6;
  uint8_t bytes[num_bytes];

  esp_wifi_get_mac(WIFI_IF_STA, bytes);

  char *p = mac_addr;
  for (auto i = 0; i < num_bytes; i++) {
    const uint8_t byte = bytes[i];

    // this is knownly duplicated code to avoid creating dependencies
    if (byte < 0x10)
      *p++ = '0';                      // zero pad when less than 0x10
    itoa(byte, p, 16);                 // convert to hex
    p = (byte < 0x10) ? p + 1 : p + 2; // move pointer forward based on zero padding
  }

  return mac_addr;
}

void Net::setName(const char *name) {
  auto &hostname = __singleton__._hostname;

  memccpy(hostname, name, 0x00, max_name_and_id_len);

  ESP_LOGI(TAG, "assigned name [%s]", hostname);

  esp_netif_set_hostname(__singleton__.netif_, hostname);
}

bool Net::start(const Opts &opts) {
  Net &net = __singleton__;

  net._opts = opts;

  esp_err_t rc = ESP_OK;

  // get the network initialized
  net.init();

  rc = esp_wifi_set_mode(WIFI_MODE_STA);
  checkError(__PRETTY_FUNCTION__, rc);

  auto powersave = WIFI_PS_NONE;
  // auto powersave = WIFI_PS_MIN_MODEM;

  rc = esp_wifi_set_ps(powersave);
  checkError(__PRETTY_FUNCTION__, rc);

  rc = esp_wifi_set_protocol(WIFI_IF_STA,
                             WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  checkError(__PRETTY_FUNCTION__, rc);

  wifi_config_t cfg;
  bzero(&cfg, sizeof(cfg));
  cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  cfg.sta.bssid_set = 0;

  memccpy(cfg.sta.ssid, opts.ssid, 0x00, sizeof(cfg.sta.ssid) - 1);
  memccpy(cfg.sta.password, opts.passwd, 0x00, sizeof(cfg.sta.password) - 1);

  // strncpy((char *)cfg.sta.ssid, Binder::wifiSsid(), sizeof(cfg.sta.ssid) - 1);
  // strncpy((char *)cfg.sta.password, Binder::wifiPasswd(), sizeof(cfg.sta.password) - 1);

  rc = esp_wifi_set_config(WIFI_IF_STA, &cfg);
  checkError(__PRETTY_FUNCTION__, rc);

  // initial hostname is the host id
  memccpy(__singleton__._hostname, hostID(), 0x00, max_name_and_id_len);

  esp_wifi_start();
  // StatusLED::brighter();

  ESP_LOGI(TAG, "standing by for IP address...");

  return true;
}

void Net::stop() {
  Net &net = __singleton__;

  net.reconnect_ = false;

  esp_wifi_disconnect();
  esp_wifi_stop();
}

void Net::wifi_events(void *ctx, esp_event_base_t base, int32_t id, void *data) {

  Net *net = (Net *)ctx;

  if (ctx == nullptr) {
    return;
  }

  switch (id) {
  case WIFI_EVENT_STA_START:
    esp_netif_set_hostname(net->netif_, Net::hostID());
    esp_wifi_connect();
    break;

  case WIFI_EVENT_STA_CONNECTED:
    net->connected(data);
    break;

  case WIFI_EVENT_STA_DISCONNECTED:
    net->disconnected(data);
    break;

  case WIFI_EVENT_STA_STOP:
    break;

  default:
    break;
  }
}

} // namespace ruth
