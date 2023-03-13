
//  Ruth
//  Copyright (C) 2020  Tim Hughey
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

#include "binder/binder.hpp"
#include "ArduinoJson.h"

#include <algorithm>
#include <array>
#include <esp_attr.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <time.h>

namespace ruth {

extern const std::uint8_t raw_start[] asm("_binary_binder_desk_mp_start");
extern const std::size_t raw_bytes asm("binder_desk_mp_length");

static constexpr std::size_t CAPACITY{128 * 7};

Binder::Binder() noexcept
    : doc(CAPACITY), _mac_address{0x00}, _host_id(CONFIG_LWIP_LOCAL_HOSTNAME) {

  esp_netif_init(); // must initialize netif to get mac address

  // need mac address to make host id
  check_error(esp_read_mac(_mac_address.data(), ESP_MAC_WIFI_STA), "read mac");

  // finish building host id
  _host_id.append(mac_address(4, "-"));

  ESP_LOGI(TAG, "host_id=%s", _host_id.c_str());

  // parse the binder (also needed for calculating hostname)
  parse();
}

void Binder::check_error(esp_err_t err, const char *desc) {
  if (err == ESP_OK) return;

  ESP_LOGE(TAG, "%s (%s)", esp_err_to_name(err), desc);

  vTaskDelay(pdMS_TO_TICKS(5000)); // let things settle

  esp_restart();
}

const std::string &Binder::hostname() noexcept {

  if (_hostname.empty()) {
    JsonObjectConst hosts = doc["hosts"];
    const char *v = hosts[_host_id.data()];

    _hostname = v ? std::string(v) : string(_host_id);
  }

  return _hostname;
}

const std::string Binder::mac_address(std::size_t want_bytes, csv &&sep) const noexcept {

  std::ostringstream mac;

  for (std::size_t i = 0; i < want_bytes; i++) {
    if (!sep.empty()) mac << sep;

    mac << std::hex << (int)_mac_address[i];
  }

  return mac.str();
}

void Binder::parse() {
  if (auto err = deserializeMsgPack(doc, raw_start, raw_bytes); err) {
    ESP_LOGW(TAG, "parse failed %s, bytes[%d]", err.c_str(), raw_bytes);
    vTaskDelay(portMAX_DELAY);
  }

  root = doc.as<JsonObjectConst>();
  meta = root["meta"].as<JsonObjectConst>();

  // setup for call to localtime_r ans strftime
  std::array<char, 42> binder_at{0x00};
  struct tm timeinfo = {};
  const auto mtime = meta["mtime"].as<time_t>();

  localtime_r(&mtime, &timeinfo);

  strftime(binder_at.data(), binder_at.size(), "%c", &timeinfo);

  ESP_LOGI(TAG, "%s doc_bytes[%d/%d]", binder_at.data(), doc.memoryUsage(), CAPACITY);
}

} // namespace ruth
