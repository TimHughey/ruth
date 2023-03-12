
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
#include <mdns.h>
#include <optional>
#include <stdio.h>

namespace ruth {
namespace shared {
DRAM_ATTR std::optional<LightDesk> desk;
TaskHandle_t desk_task;
} // namespace shared

static esp_timer_handle_t destruct_timer;
static void self_destruct(void *desk_v) noexcept {
  esp_timer_delete(std::exchange(destruct_timer, nullptr));
  shared::desk.reset();
}

LightDesk::LightDesk() noexcept
    : acceptor{io_ctx, tcp_endpoint{ip_tcp::v4(), SERVICE_PORT}} //
{
  ESP_LOGD(TAG.data(), "enabled, starting up");

  auto rc =                   // create the task using a static desk_stack
      xTaskCreate(&task_main, // static func to start task
                  TAG.data(), // task name
                  10 * 1024,  // desk_stack size
                  this,       // none, use shared::desk
                  7,          // priority
                  &shared::desk_task);

  ESP_LOGD(TAG.data(), "started rc=%d task=%p", rc, shared::desk_task);
}

void LightDesk::advertise() noexcept {
  const auto host = net::hostname();
  const auto mac_addr = net::mac_address();

  if ((mdns_init() == ESP_OK) && (mdns_hostname_set(host) == ESP_OK)) {
    char *n = nullptr;

    if (auto bytes = asprintf(&n, "%s@%s", mac_addr, host); bytes != -1) {
      auto name = std::unique_ptr<char>(n);

      if (mdns_instance_name_set(name.get()) == ESP_OK) {
        ESP_LOGD(LightDesk::TAG.data(), "host[%s] instance[%s]", host, name.get());
        auto txt_data = std::array<mdns_txt_item_t, 1>{{"desk", "true"}};
        mdns_service_add(name.get(), LightDesk::SERVICE_NAME.data(),
                         LightDesk::SERVICE_PROTOCOL.data(), LightDesk::SERVICE_PORT,
                         txt_data.data(), txt_data.size());
      } else {
        ESP_LOGE(LightDesk::TAG.data(), "mdns_instance_name_set() failed\n");
      }
    } else {
      ESP_LOGE(LightDesk::TAG.data(), "asprintf failed()\n");
    }
  } else {
    ESP_LOGE(LightDesk::TAG.data(), "mdns_init() or mdns_hostname_set() failed\n");
  }
}

void LightDesk::async_accept() noexcept {
  // this is the socket for the next accepted connection, store it in an
  // optional for the lamba
  peer.emplace(io_ctx);

  // since the socket is wrapped in the optional and asyncLoop wants the actual
  // socket we must deference or get the value of the optional
  acceptor.async_accept(*peer, [this](const error_code ec) {
    if (ec) return; // no more work

    if (shared::active_session.has_value()) {
      shared::active_session.reset();
    }

    // io::log_accept_socket(TAG, "ctrl"sv, *peer, peer->remote_endpoint());

    peer->set_option(ip_tcp::no_delay(true));

    shared::active_session.emplace(std::move(*peer));

    async_accept();
  });
}

void LightDesk::run() noexcept {}

void IRAM_ATTR LightDesk::task_main(void *desk_v) noexcept {
  auto *desk = static_cast<LightDesk *>(desk_v);

  desk->advertise();
  desk->async_accept();
  desk->io_ctx.run();

  ESP_LOGI(LightDesk::TAG.data(), "io_ctx work exhausted");

  const esp_timer_create_args_t args{.callback = self_destruct,
                                     .arg = desk,
                                     .dispatch_method = ESP_TIMER_TASK,
                                     .name = "desk::destruct",
                                     .skip_unhandled_events = false};

  if (auto rc = esp_timer_create(&args, &destruct_timer); rc == ESP_OK) {
    esp_timer_start_once(destruct_timer, 1000);
  }

  vTaskDelete(shared::desk_task);
}

} // namespace ruth
