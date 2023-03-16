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

#include "ArduinoJson.h"
#include "binder/binder.hpp"
#include "ru_base/types.hpp"

#include <array>
#include <cstdlib>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <future>
#include <memory>
#include <string>
#include <string_view>

namespace ruth {

class Net;

class Net {
public:
  enum Notifies : uint32_t { IP_READY = 0x01, SNTP_READY = 0x02 };

public:
  Net(Binder *binder) noexcept;
  ~Net() = default;

  const std::string ip4_address() noexcept;

  void wait_for_ready() noexcept;

private:
  void acquired_ip(void *event_data) noexcept;
  static void acquired_time(struct timeval *tv) noexcept; // kludge due to ESP-IDF api

  void check_error(esp_err_t err, const auto *desc) noexcept;

  static void events(void *ctx, esp_event_base_t base, int32_t id, void *data) noexcept;

  void sntp() noexcept;

private:
  // order dependent
  esp_netif_t *netif;
  TaskHandle_t notify_task;

  // order independent
  std::array<string, 2> sntp_servers;
  uint32_t ip_timeout_ticks;
  uint32_t sntp_timeout_ticks;

  static Net *self; // need this for static sntp callback

public:
  const std::string hostname;
  const std::string host_id;

public:
  static constexpr const auto TAG{"Net"};
};

} // namespace ruth

//
// free functions
//
