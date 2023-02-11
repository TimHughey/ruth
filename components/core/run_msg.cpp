//  Ruth
//  Copyright (C) 2021  Tim Hughey
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

#include "core/run_msg.hpp"

#include <esp_system.h>
#include <esp_wifi.h>

namespace ruth {
namespace message {

Run::Run() {
  _filter.addLevel("host");
  _filter.addLevel("run");
}

void Run::assembleData(JsonObject &data) {
  wifi_ap_record_t access_pt = {};
  auto ap_rc = esp_wifi_sta_get_ap_info(&access_pt);

  if (ap_rc == ESP_OK) {
    JsonObject ap = data.createNestedObject("ap");
    JsonArray bssid = ap.createNestedArray("bssid");

    for (auto i = 0; i < 6; i++) {
      bssid.add(access_pt.bssid[i]);
    }

    ap["rssi"] = access_pt.rssi;
    ap["pri_chan"] = access_pt.primary;
  }

  JsonObject heap = data.createNestedObject("heap");
  heap["min"] = esp_get_minimum_free_heap_size();
  heap["free"] = esp_get_free_heap_size();
  auto max_alloc = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  _heap_low = (max_alloc < 5120) ? true : false;

  heap["max_alloc"] = max_alloc;
}
} // namespace message

} // namespace ruth
