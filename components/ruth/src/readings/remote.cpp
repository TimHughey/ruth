/*
    remote.cpp - Ruth Reading
    Copyright (C) 2017  Tim Hughey

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

#include <cstdlib>
#include <ctime>

#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>

#include "readings/remote.hpp"

namespace ruth {
namespace reading {
Remote::Remote() : Reading(REMOTE) { grabMetrics(); };

Remote::Remote(ReadingType_t type) : Reading(type) { grabMetrics(); }

void Remote::populateMessage(JsonDocument &doc) {
  bssid_.printf("%02x:%02x:%02x:%02x:%02x:%02x", ap_.bssid[0], ap_.bssid[1],
                ap_.bssid[2], ap_.bssid[3], ap_.bssid[4], ap_.bssid[5]);

  doc["bssid"] = bssid_.c_str();
  doc["ap_rssi"] = ap_.rssi;
  doc["ap_pri_chan"] = ap_.primary;
  doc["heap_free"] = heap_free_;
  doc["heap_min"] = heap_min_;
  doc["uptime_us"] = uptime_us_;
}

//
// Private
//
void Remote::grabMetrics() {
  ap_rc_ = esp_wifi_sta_get_ap_info(&ap_);

  if (ap_rc_ != ESP_OK) {
    bzero(&ap_, sizeof(ap_));
  }

  heap_free_ = esp_get_free_heap_size();
  heap_min_ = esp_get_minimum_free_heap_size();
  uptime_us_ = esp_timer_get_time();
}
} // namespace reading
} // namespace ruth
