
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

#include "lightdesk/lightdesk.hpp"
#include "binder/binder.hpp"
#include "dmx/dmx.hpp"
#include "io/io.hpp"
#include "network/network.hpp"
#include "ru_base/rut.hpp"
#include "session/session.hpp"

#include <array>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iostream>
#include <mdns.h>
#include <optional>
#include <sstream>
#include <stdio.h>

namespace ruth {

LightDesk::LightDesk() noexcept : acceptor{io_ctx, tcp_endpoint{ip_tcp::v4(), SERVICE_PORT}} {}

void LightDesk::advertise(Binder *binder) noexcept {
  const auto host = binder->hostname();
  const auto mac_addr = binder->mac_address();

  if ((mdns_init() == ESP_OK) && (mdns_hostname_set(host.c_str()) == ESP_OK)) {
    std::ostringstream name_ss;

    name_ss << mac_addr << "@" << host;

    string name(name_ss.str());

    if (mdns_instance_name_set(name.c_str()) == ESP_OK) {
      ESP_LOGI(LightDesk::TAG, "host[%s] instance[%s]", host.c_str(), name.c_str());

      auto txt_data = std::array<mdns_txt_item_t, 1>{{"desk", "true"}};
      mdns_service_add(name.c_str(), SERVICE_NAME.data(), SERVICE_PROTOCOL.data(), SERVICE_PORT,
                       txt_data.data(), txt_data.size());
    } else {
      ESP_LOGE(TAG, "mdns_instance_name_set() failed, name=%s", name.c_str());
    }

  } else {
    ESP_LOGE(TAG, "mdns_init() or mdns_hostname_set() failed\n");
  }
}

void LightDesk::async_accept() noexcept {

  // upon a new accepted connection create the socket with the session io_ctx
  acceptor.async_accept(io_ctx_session, [this](const error_code &ec, tcp_socket peer) {
    if (ec) return; // no more work

    session.reset(); // support only a single session

    session = std::make_unique<desk::Session>(io_ctx_session, std::move(peer));

    async_accept();
  });
}

void LightDesk::run(Binder *binder) noexcept {
  ESP_LOGI(TAG, "starting up, task=%p", xTaskGetCurrentTaskHandle());

  // add work for the io_ctx
  advertise(binder);
  async_accept();

  io_ctx.run();
  ESP_LOGI(TAG, "io_ctx work exhausted");
}

} // namespace ruth
