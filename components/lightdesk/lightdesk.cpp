
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
#include "lightdesk/advertise.hpp"
#include "ru_base/time.hpp"
#include "session/session.hpp"

#include <array>
#include <asio/placeholders.hpp>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <functional>
#include <optional>

namespace ruth {
namespace shared {
DRAM_ATTR std::optional<LightDesk> desk;
TaskHandle_t desk_task;
} // namespace shared

namespace desk {
DRAM_ATTR static StaticTask_t desk_tcb;
DRAM_ATTR static std::array<StackType_t, 10 * 1024> desk_stack;
} // namespace desk

static void _run(void *) noexcept {
  desk::Advertise::create(LightDesk::SERVICE_PORT)->init();

  shared::desk->run();
  shared::desk.reset();

  vTaskDelete(shared::desk_task);
}

LightDesk::LightDesk() noexcept
    : acceptor{io_ctx, tcp_endpoint{ip_tcp::v4(), SERVICE_PORT}} //
{
  ESP_LOGI(TAG.data(), "enabled, starting up");

  shared::desk_task =                            // create the task using a static desk_stack
      xTaskCreateStatic(_run,                    // static func to start task
                        TAG.data(),              // task name
                        desk::desk_stack.size(), // desk_stack size
                        nullptr,                 // none, use shared::desk
                        4,                       // priority
                        desk::desk_stack.data(), // static task desk_stack
                        &desk::desk_tcb          // task control block
      );

  ESP_LOGI(TAG.data(), "started desk_tcb=%p", &desk::desk_tcb);
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

// run the lightdesk
void LightDesk::run() {
  async_accept();

  io_ctx.run();

  ESP_LOGI(TAG.data(), "io_ctx work exhausted");
}

} // namespace ruth
