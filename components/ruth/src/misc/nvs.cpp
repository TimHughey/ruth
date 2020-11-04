/*
    nvs.cpp - Abstraction for ESP NVS
    Copyright (C) 2019  Tim Hughey

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

#include "misc/nvs.hpp"
#include "protocols/mqtt.hpp"

namespace ruth {
static NVS_t *__singleton__ = nullptr;
static const char TAG[] = "NVS";

NVS::NVS() {
  esp_err_t _esp_rc = ESP_OK;
  _esp_rc = nvs_flash_init();

  ESP_LOGI(TAG, "[%s] nvs_flash_init()", esp_err_to_name(_esp_rc));

  switch (_esp_rc) {
  case ESP_ERR_NVS_NO_FREE_PAGES:
    ESP_LOGW(TAG, "nvs no free pages, will erase");
    break;
  case ESP_ERR_NVS_NEW_VERSION_FOUND:
    ESP_LOGW(TAG, "nvs new data version, must erase");
    break;
  default:
    break;
  }

  if ((_esp_rc == ESP_ERR_NVS_NO_FREE_PAGES) ||
      (_esp_rc == ESP_ERR_NVS_NEW_VERSION_FOUND)) {

    // erase and attempt initialization again
    _esp_rc = nvs_flash_erase();
    ESP_LOGW(TAG, "[%s] nvs_flash_erase()", esp_err_to_name(_esp_rc));

    if (_esp_rc == ESP_OK) {
      _esp_rc = nvs_flash_init();

      ESP_LOGW(TAG, "[%s] nvs_init() (second attempt)",
               esp_err_to_name(_esp_rc));
    }
  }
}

// STATIC
NVS_t *NVS::init() { return NVS::_instance_(); }

// STATIC
NVS_t *NVS::_instance_() {
  if (__singleton__ == nullptr) {
    __singleton__ = new NVS();
  }
  return __singleton__;
}

NVS::~NVS() {
  if (__singleton__) {

    delete __singleton__;
    __singleton__ = nullptr;
  }
}

// void NVS::publishMsg(const char *key, NVSMessage_t *blob) {
//   unique_ptr<struct tm> timeinfo(new struct tm);
//
//   localtime_r(&(blob->time), timeinfo.get());
//
//   Text::rlog(timeinfo.get(), "key(%s) msg(%s)", key, blob->msg);
// }

} // namespace ruth
