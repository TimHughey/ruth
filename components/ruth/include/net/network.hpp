#ifndef _ruth_network_hpp
#define _ruth_network_hpp

#include <cstdlib>

#include <esp_attr.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <time.h>

#include "local/types.hpp"

namespace ruth {

typedef class Net Net_t;
class Net {
public:
  Net(); // SINGLETON
  void ensureTimeIsSet();
  static void earlyInit() { _instance_()->evg_ = xEventGroupCreate(); }
  static bool start() { return _instance_()->_start_(); };

  static void stop();
  static EventGroupHandle_t eventGroup();

  // hostname and mac address
  static const char *hostname() { return _instance_()->name_.c_str(); }
  static const char *hostID();
  static void setName(const char *name);
  static const char *macAddress();

  //  static const char *dnsIP() { return _instance_()->dns_str_; };

  static const char *ca_start() { return (const char *)_ca_start_; };
  static const uint8_t *ca_end() { return _ca_end_; };

  static bool hostIdAndNameAreEqual();

  // static void resumeNormalOps();
  // static void suspendNormalOps();
  static bool waitForConnection(uint32_t wait_ms = UINT32_MAX);
  static bool waitForInitialization(uint32_t wait_ms = UINT32_MAX);
  static bool waitForIP(uint32_t wait_ms = 60000);
  static bool waitForName(uint32_t wait_ms = UINT32_MAX);
  static bool waitForNormalOps(uint32_t wait_ms = UINT32_MAX);
  static bool isTimeSet();
  static bool waitForReady(uint32_t wait_ms = UINT32_MAX);
  static bool waitForTimeset(uint32_t wait_ms = UINT32_MAX);
  static void clearTransportReady() { setTransportReady(false); };
  static void setTransportReady(bool val = true);

  static inline bool clearBits() { return true; };
  static inline bool noClearBits() { return false; };
  static inline bool waitAllBits() { return true; };
  static inline bool waitAnyBits() { return false; };

  static EventBits_t connectedBit() { return BIT0; };
  static EventBits_t ipBit() { return BIT1; };
  static EventBits_t nameBit() { return BIT2; };
  static EventBits_t normalOpsBit() { return BIT3; };
  static EventBits_t readyBit() { return BIT4; };
  static EventBits_t timeSetBit() { return BIT5; };
  static EventBits_t initializedBit() { return BIT6; };
  static EventBits_t transportBit() { return BIT7; };

  static const char *disconnectReason(wifi_err_reason_t reason);

  static const char *tagEngine() { return (const char *)"Net"; };

private: // member functions
  void acquiredIP(void *event_data);

  static void checkError(const char *func, esp_err_t err);
  void connected(void *event_data);

  // delay task for milliseconds
  void delay(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

  void disconnected(void *event_data);
  void init();

  // object specific methods of static methods
  const char *_dnsIP_();
  static Net_t *_instance_();
  bool _start_();

  // Event Handlers
  static void ip_events(void *ctx, esp_event_base_t base, int32_t id,
                        void *data);
  static void wifi_events(void *ctx, esp_event_base_t base, int32_t id,
                          void *data);

private:
  EventGroupHandle_t evg_;
  esp_err_t init_rc_ = ESP_FAIL;
  esp_netif_t *netif_ = nullptr;
  // esp_netif_ip_info_t ip_info_;
  // esp_netif_dns_info_t primary_dns_;
  // char dns_str_[16] = {};

  TextBuffer<20> mac_;
  TextBuffer<25> host_id_;
  TextBuffer<35> name_;
  bool reconnect_ = true;

  static const uint8_t _ca_start_[] asm("_binary_ca_pem_start");
  static const uint8_t _ca_end_[] asm("_binary_ca_pem_end");
};
} // namespace ruth

#endif
