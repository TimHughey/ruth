#include <cstdlib>
#include <memory>
#include <string>

#include <esp_attr.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <sys/time.h>
#include <time.h>

#include "lwip/apps/sntp.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "core/binder.hpp"
#include "misc/restart.hpp"
#include "misc/status_led.hpp"
#include "net/network.hpp"

extern "C" {
int setenv(const char *envname, const char *envval, int overwrite);
void tzset(void);
}

using std::unique_ptr;

namespace ruth {

static Net_t *__singleton__ = nullptr;

Net::Net() { evg_ = xEventGroupCreate(); }

void Net::acquiredIP(void *event_data) {
  esp_netif_get_ip_info(netif_, &ip_info_);
  esp_netif_get_dns_info(netif_, ESP_NETIF_DNS_MAIN, &primary_dns_);

  auto *dns_ip = (uint8_t *)&(primary_dns_.ip);
  snprintf(dns_str_, sizeof(dns_str_), IPSTR, dns_ip[0], dns_ip[1], dns_ip[2],
           dns_ip[3]);

  xEventGroupSetBits(evg_, ipBit());
}

// STATIC!!
void Net::checkError(const char *func, esp_err_t err) {
  if (err == ESP_OK) {
    return;
  }

  const size_t max_msg_len = 256;
  char *msg = new char[max_msg_len];

  vTaskDelay(pdMS_TO_TICKS(1000)); // let things settle

  snprintf(msg, max_msg_len, "[%s] %s", esp_err_to_name(err), func);
  ESP_LOGE(tagEngine(), "%s", msg);

  Restart::restart(msg);
}

void Net::connected(void *event_data) {
  xEventGroupSetBits(evg_, connectedBit());
}

void Net::disconnected(void *event_data) {

  xEventGroupClearBits(evg_, connectedBit());

  if (reconnect_) {
    esp_wifi_connect();
  }
}

// STATIC!!
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

// STATIC!!
void Net::wifi_events(void *ctx, esp_event_base_t base, int32_t id,
                      void *data) {

  Net_t *net = (Net_t *)ctx;

  if (ctx == nullptr) {
    return;
  }

  switch (id) {
  case WIFI_EVENT_STA_START:
    esp_netif_set_hostname(_instance_()->netif_, Net::hostID().c_str());
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

void Net::stop() {
  _instance_()->reconnect_ = false;

  esp_wifi_disconnect();
  esp_wifi_stop();
}

EventGroupHandle_t Net::eventGroup() { return _instance_()->evg_; }

Net_t *Net::_instance_() {
  if (__singleton__ == nullptr) {
    __singleton__ = new Net();
  }

  return __singleton__;
}

void Net::init() {
  esp_err_t rc = ESP_OK;

  if (init_rc_ == ESP_OK)
    return;

  esp_netif_init();

  rc = esp_event_loop_create_default();
  checkError(__PRETTY_FUNCTION__, rc); // never returns if rc != ESP_OK
  netif_ = esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  rc = esp_wifi_init(&cfg);
  checkError(__PRETTY_FUNCTION__, rc);

  rc = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_events,
                                  _instance_());
  checkError(__PRETTY_FUNCTION__, rc);

  rc = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_events,
                                  _instance_());
  checkError(__PRETTY_FUNCTION__, rc);

  rc = esp_wifi_set_storage(WIFI_STORAGE_FLASH);
  checkError(__PRETTY_FUNCTION__, rc);

  init_rc_ = rc;
}

// polls the current time waiting for SNTP to complete or timeout is reached
void Net::ensureTimeIsSet() {
  elapsedMillis sntp_elapsed;
  const uint32_t total_ms = 30000;
  const uint32_t check_ms = 2998;

  for (auto wait_sntp = true; wait_sntp;) {
    struct timeval curr_time = {};

    uint32_t recent_time = 1591114560;

    StatusLED::percent(0.10);

    delay(check_ms / 2);
    StatusLED::percent(0.75);
    delay(check_ms / 2);

    gettimeofday(&curr_time, nullptr);

    if ((curr_time.tv_sec > recent_time) || (sntp_elapsed > total_ms)) {
      wait_sntp = false;
    }
  }

  // one final wait to ensure SNTP has fully set time
  StatusLED::percent(0.10);
  delay(check_ms / 2);
  StatusLED::percent(0.75);
  delay(check_ms / 2);

  if (sntp_elapsed > total_ms) {
    Restart::restart("SNTP failed");
  } else {
    xEventGroupSetBits(evg_, timeSetBit());
    ESP_LOGI(tagEngine(), "SNTP complete[%0.1fs]", sntp_elapsed.toSeconds());
  }
}

const string_t &Net::hostID() {
  static string_t _host_id;

  if (_host_id.length() == 0) {
    _host_id = "ruth.";
    _host_id += macAddress();
  }

  return _host_id;
}

const string_t &Net::macAddress() {
  static string_t _mac;

  // must wait for initialization of wifi before providing mac address
  waitForInitialization();

  if (_mac.length() == 0) {
    unique_ptr<char[]> buf(new char[24]);
    uint8_t mac[6] = {};

    esp_wifi_get_mac(WIFI_IF_STA, mac);

    sprintf(buf.get(), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2],
            mac[3], mac[4], mac[5]);

    // bytes << std::hex << std::setfill('0');
    // for (int i = 0; i <= 5; i++) {
    //   bytes << std::setw(sizeof(uint8_t) * 2) <<
    //   static_cast<unsigned>(mac[i]);
    // }
    //
    _mac = buf.get();
  }

  return _mac;
};

void Net::setName(const char *name) {

  _instance_()->name_ = name;
  ESP_LOGI(tagEngine(), "assigned name [%s]", _instance_()->name_.c_str());

  esp_netif_set_hostname(_instance_()->netif_, _instance_()->name_.c_str());

  xEventGroupSetBits(_instance_()->eventGroup(), nameBit());
}

bool Net::_start_() {
  esp_err_t rc = ESP_OK;

  // get the network initialized
  init();

  rc = esp_wifi_set_mode(WIFI_MODE_STA);
  checkError(__PRETTY_FUNCTION__, rc);

  auto powersave = WIFI_PS_NONE;
  // auto powersave = WIFI_PS_MIN_MODEM;

  rc = esp_wifi_set_ps(powersave);
  checkError(__PRETTY_FUNCTION__, rc);

  rc = esp_wifi_set_protocol(
      WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  checkError(__PRETTY_FUNCTION__, rc);

  wifi_config_t cfg;
  bzero(&cfg, sizeof(cfg));
  cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  cfg.sta.bssid_set = 0;
  strncpy((char *)cfg.sta.ssid, Binder::wifiSsid(), sizeof(cfg.sta.ssid));
  strncpy((char *)cfg.sta.password, Binder::wifiPasswd(),
          sizeof(cfg.sta.password));

  rc = esp_wifi_set_config(WIFI_IF_STA, &cfg);
  checkError(__PRETTY_FUNCTION__, rc);

  // wifi is initialized so signal to processes waiting they can continue
  xEventGroupSetBits(evg_, initializedBit());

  // finish constructing our initial hostname
  name_.append(macAddress());

  esp_wifi_start();
  StatusLED::brighter();

  ESP_LOGI(tagEngine(), "standing by for IP address...");
  if (waitForIP()) {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    sntp_setservername(0, Binder::ntpServer(0));
    sntp_setservername(1, Binder::ntpServer(1));
    sntp_init();

    ensureTimeIsSet();

    // NOTE: once we've reached here the network is connected, ip address
    //       acquired and the time is set -- signal to other tasks
    //       we are ready for normal operations
    xEventGroupSetBits(evg_, (readyBit() | normalOpsBit()));
  } else {
    Restart::restart("IP address assignment failed");
  }

  return true;
}

void Net::resumeNormalOps() {
  xEventGroupSetBits(_instance_()->eventGroup(), Net::normalOpsBit());
}

void Net::suspendNormalOps() {
  xEventGroupClearBits(_instance_()->eventGroup(), Net::normalOpsBit());
}

// wait_ms defaults to UINT32_MAX
bool Net::waitForConnection(uint32_t wait_ms) {
  EventBits_t wait_bit = connectedBit();
  EventGroupHandle_t eg = _instance_()->eventGroup();
  uint32_t wait_ticks =
      (wait_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(wait_ms);
  EventBits_t bits_set;

  // set status LED to 75% while waiting for WiFi
  StatusLED::brighter();
  bits_set = xEventGroupWaitBits(eg, wait_bit, noClearBits(), waitAllBits(),
                                 wait_ticks);

  return (bits_set & wait_bit) ? true : false;
}

// wait_ms defaults to UINT32_MAX
bool Net::waitForInitialization(uint32_t wait_ms) {
  EventBits_t wait_bit = initializedBit();
  EventGroupHandle_t eg = _instance_()->eventGroup();
  uint32_t wait_ticks =
      (wait_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(wait_ms);
  EventBits_t bits_set;

  bits_set = xEventGroupWaitBits(eg, wait_bit, noClearBits(), waitAllBits(),
                                 wait_ticks);

  return (bits_set & wait_bit) ? true : false;
}

// wait_ms defaults to 10 seconds
bool Net::waitForIP(uint32_t wait_ms) {
  EventBits_t wait_bit = ipBit();
  EventGroupHandle_t eg = _instance_()->eventGroup();
  uint32_t wait_ticks =
      (wait_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(wait_ms);
  EventBits_t bits_set;

  bits_set = xEventGroupWaitBits(eg, wait_bit, noClearBits(), waitAllBits(),
                                 wait_ticks);

  return (bits_set & wait_bit) ? true : false;
}

// wait_ms defaults to zero
bool Net::waitForName(uint32_t wait_ms) {
  EventBits_t wait_bit = nameBit();
  EventGroupHandle_t eg = _instance_()->eventGroup();
  uint32_t wait_ticks =
      (wait_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(wait_ms);
  EventBits_t bits_set;

  bits_set = xEventGroupWaitBits(eg, wait_bit, noClearBits(), waitAllBits(),
                                 wait_ticks);

  return (bits_set & wait_bit) ? true : false;
}

// wait_ms defaults to portMAX_DELAY when not passed
bool Net::waitForNormalOps(uint32_t wait_ms) {
  EventBits_t wait_bit = connectedBit() | transportBit() | normalOpsBit();
  EventGroupHandle_t eg = _instance_()->eventGroup();
  uint32_t wait_ticks =
      (wait_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(wait_ms);
  EventBits_t bits_set;

  bits_set = xEventGroupWaitBits(eg, wait_bit, noClearBits(), waitAllBits(),
                                 wait_ticks);

  return (bits_set & wait_bit) ? true : false;
}

bool Net::isTimeSet() {
  EventBits_t wait_bit = timeSetBit();
  EventGroupHandle_t eg = _instance_()->eventGroup();
  uint32_t wait_ticks = 0;
  EventBits_t bits_set;

  // xEventGroupWaitBits returns the bits set in the event group even if
  // the wait times out (which we want in this case if it's not set)
  bits_set = xEventGroupWaitBits(eg, wait_bit, noClearBits(), waitAllBits(),
                                 wait_ticks);

  return (bits_set & wait_bit) ? true : false;
}

// intended use is to signal to tasks that require WiFi but not
// normalOps
// wait_ms defaults to portMAX_DELAY
bool Net::waitForReady(uint32_t wait_ms) {
  EventBits_t wait_bit = connectedBit() | ipBit() | readyBit();
  EventGroupHandle_t eg = _instance_()->eventGroup();
  uint32_t wait_ticks =
      (wait_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(wait_ms);
  EventBits_t bits_set;

  bits_set = xEventGroupWaitBits(eg, wait_bit, noClearBits(), waitAllBits(),
                                 wait_ticks);

  return (bits_set & wait_bit) ? true : false;
}

// wait_ms defaults to portMAX_DELAY
bool Net::waitForTimeset(uint32_t wait_ms) {
  EventBits_t wait_bit = timeSetBit();
  EventGroupHandle_t eg = _instance_()->eventGroup();
  uint32_t wait_ticks =
      (wait_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(wait_ms);
  EventBits_t bits_set;

  bits_set = xEventGroupWaitBits(eg, wait_bit, noClearBits(), waitAllBits(),
                                 wait_ticks);

  return (bits_set & wait_bit) ? true : false;
}

void Net::setTransportReady(bool val) {
  if (val) {
    xEventGroupSetBits(_instance_()->eventGroup(), transportBit());
  } else {
    xEventGroupClearBits(_instance_()->eventGroup(), transportBit());
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
} // namespace ruth
