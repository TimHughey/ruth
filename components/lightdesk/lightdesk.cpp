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

#include "lightdesk/lightdesk.hpp"
#include "dmx/dmx.hpp"
#include "io/io.hpp"
#include "lightdesk/advertise.hpp"
#include "ru_base/time.hpp"
#include "server/server.hpp"

#include <array>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <optional>

namespace ruth {
namespace shared {
DRAM_ATTR std::optional<LightDesk> desk;
DRAM_ATTR std::optional<desk::Server> desk_server;
TaskHandle_t desk_task;
} // namespace shared

namespace desk {
DRAM_ATTR static StaticTask_t desk_tcb;
DRAM_ATTR static std::array<StackType_t, 10 * 1024> desk_stack;
} // namespace desk

// static method for create, access and reset of shared LightDesk
LightDesk &LightDesk::create(const LightDesk::Opts &opts) { // static
  return shared::desk.emplace(opts);
}

void LightDesk::reset() { // static
  shared::desk_server->shutdown();
  shared::desk->io_ctx.stop();

  shared::desk_server.reset();
  shared::desk.reset();
}

// run the lightdesk
void LightDesk::_run([[maybe_unused]] void *data) {
  shared::desk_server.emplace(                               //
      desk::server::Inject{shared::desk->io_ctx,             //
                           SERVICE_PORT,                     //
                           shared::desk->opts.idle_shutdown} //
  );

  desk::Advertise::create(shared::desk_server->localPort())->init();
  shared::desk_server->asyncLoop(); // schedule accept connections

  shared::desk->io_ctx.run();

  ESP_LOGI(TAG.data(), "run() io_ctx work exhausted");

  vTaskDelete(shared::desk_task);
}

// general API
void LightDesk::init() {
  ESP_LOGI(TAG.data(), "enabled, starting up");

  shared::desk_task =                            // create the task using a static desk_stack
      xTaskCreateStatic(&LightDesk::_run,        // static func to start task
                        TAG.data(),              // task name
                        desk::desk_stack.size(), // desk_stack size
                        nullptr,                 // none, use shared::desk
                        4,                       // priority
                        desk::desk_stack.data(), // static task desk_stack
                        &desk::desk_tcb          // task control block
      );

  ESP_LOGI(TAG.data(), "started desk_tcb=%p", &desk::desk_tcb);
}

} // namespace ruth
