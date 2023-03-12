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

#pragma once

#include <cstdlib>
#include <esp_event.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace ruth {

using src_loc = std::source_location;
typedef const std::string_view csv;

class Net;

namespace shared {
extern std::unique_ptr<Net> net;
}

class Net {
public:
  struct Opts {
    Opts() = default;
    Opts(const char *ssid, const char *passwd, int32_t ip_max_wait) noexcept
        : ssid(ssid),                              //
          passwd(passwd),                          //
          ip_max_wait(ip_max_wait),                //
          notify_task(xTaskGetCurrentTaskHandle()) //
    {}

    ~Opts() = default;

    const char *ssid{nullptr};
    const char *passwd{nullptr};
    int32_t ip_max_wait{0};
    TaskHandle_t notify_task{nullptr};
  };

public:
  enum Notifies : uint32_t { READY = 0x01 };

public:
  Net(Opts &&opts) noexcept;
  ~Net() = default;

  static const std::string ip4_address() noexcept;
  const char *name(const char *new_name) noexcept;

private:
  void acquired_ip(void *event_data) noexcept;
  void check_error(esp_err_t err, const src_loc loc = src_loc()) noexcept;

  static void events(void *ctx, esp_event_base_t base, int32_t id, void *data) noexcept;

private:
  // order dependent
  Opts opts;
  esp_netif_t *netif;

public:
  std::vector<char> mac_address{};
  std::string host_id;
  std::string hostname;

public:
  static const char ca_begin[] asm("_binary_ca_pem_start");
  static const char ca_end[] asm("_binary_ca_pem_end");
  static constexpr const char *TAG{"Net"};
};

////
//// Ruth net free functions
////

namespace net {

inline const char *ca_begin() noexcept { return &(shared::net->ca_begin[0]); }
inline const char *ca_end() noexcept { return &(shared::net->ca_end[0]); }

inline const char *host_id() noexcept { return shared::net->host_id.data(); }
inline const char *hostname(const char *new_name = nullptr) noexcept {
  return shared::net->name(new_name);
}
inline const char *mac_address() noexcept { return shared::net->mac_address.data(); }
inline bool has_assigned_name() noexcept { return shared::net->host_id != shared::net->hostname; }

} // namespace net
} // namespace ruth
