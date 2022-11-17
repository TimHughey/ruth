/*
    lightdesk/lightdesk.cpp - Ruth Light Desk
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

#include "lightdesk/advertise.hpp"
#include "network/network.hpp"
#include "ru_base/types.hpp"

#include "esp_log.h"
#include "mdns.h"
#include <memory>
#include <stdio.h> // asprintf
#include <vector>

namespace ruth {
namespace shared {
desk::shAdvertise __desk_advertise;
}

namespace desk {

shAdvertise Advertise::create(const Port service_port) { // static
  shared::__desk_advertise = shAdvertise(new Advertise(service_port));
  return ptr();
}
shAdvertise Advertise::ptr() { return shared::__desk_advertise; }
void Advertise::reset() { shared::__desk_advertise.reset(); } // reset (deallocate) shared instance

// general API

shAdvertise Advertise::init() {
  const auto host = Net::hostname();
  const auto mac_addr = Net::macAddress();

  if ((mdns_init() == ESP_OK) && (mdns_hostname_set(host) == ESP_OK)) {
    char *n = nullptr;

    if (auto bytes = asprintf(&n, "%s@%s", mac_addr, host); bytes != -1) {
      name = std::unique_ptr<char>(n);

      if (mdns_instance_name_set(name.get()) == ESP_OK) {
        ESP_LOGI(TAG.data(), "host=%s instance=%s", Net::hostname(), name.get());
        auto txt_data = std::array<mdns_txt_item_t, 1>{{"desk", "true"}};
        mdns_service_add(name.get(), service.data(), protocol.data(), service_port, txt_data.data(),
                         txt_data.size());
      } else {
        ESP_LOGE(TAG.data(), "mdns_instance_name_set() failed\n");
      }
    } else {
      ESP_LOGE(TAG.data(), "asprintf failed()\n");
    }
  } else {
    ESP_LOGE(TAG.data(), "mdns_init() or mdns_hostname_set() failed\n");
  }

  return shared_from_this();
}

} // namespace desk
} // namespace ruth
