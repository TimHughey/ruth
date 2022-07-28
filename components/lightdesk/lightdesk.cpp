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
#include "msg.hpp"
#include "server/server.hpp"

#include <array>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ruth {
namespace shared {
shLightDesk __lightdesk;
shLightDesk lightdesk() { return __lightdesk; }
TaskHandle_t lightdesk_task;
} // namespace shared

namespace desk {
DRAM_ATTR static StaticTask_t tcb;
DRAM_ATTR static std::array<StackType_t, 5 * 1024> stack;
// DRAM_ATTR static HeadUnits units;

} // namespace desk

// constexpr size_t RX_MAX_LEN = 1024;
// DRAM_ATTR static std::array<char, RX_MAX_LEN> rx_buff;

// static method for create, access and reset of shared LightDesk
shLightDesk LightDesk::create(const LightDesk::Opts &opts) { // static
  shared::__lightdesk = shLightDesk(new LightDesk(opts));

  return shared::lightdesk();
}

IRAM_ATTR shLightDesk LightDesk::ptr() { // static
  return shared::lightdesk()->shared_from_this();
}

void LightDesk::reset() { // static
  shared::lightdesk().reset();
}

// general API

// IRAM_ATTR void LightDesk::idleWatchDog() {
//   if (state != desk::SHUTDOWN) {
//     // each call will restart the timer
//     idle_timer.expires_after(idle_check);

//     idle_timer.async_wait( //
//         [self = shared_from_this(), &units = shared::headunits](error_code ec) {
//           // if the timer ever expires then we're idle
//           if (!ec) {
//             std::for_each(units.begin(), units.end(), [](shHeadUnit unit) { unit->dark(); });

//             self->state.idle();

//             ESP_LOGD(TAG.data(), "is idle");

//             self->idleWatchDog(); // always reschedule self
//           } else {
//             ESP_LOGD(TAG.data(), "idleWatchDog() ec=%s", ec.message().c_str());
//           }
//         });
//   }
// }

shLightDesk LightDesk::init() {
  ESP_LOGI(TAG.data(), "enabled, starting up");

  // shared::headunits.emplace_back(std::make_shared<AcPower>("ac power"));
  // shared::headunits.emplace_back(std::make_shared<DiscoBall>("disco ball", 1)); // pwm 1
  // shared::headunits.emplace_back(std::make_shared<ElWire>("el dance", 2));      // pwm 2
  // shared::headunits.emplace_back(std::make_shared<ElWire>("el entry", 3));      // pwm 3
  // shared::headunits.emplace_back(std::make_shared<LedForest>("led forest", 4)); // pwm 4

  shared::lightdesk_task =                  // create the task using a static stack
      xTaskCreateStatic(&LightDesk::_run,   // static func to start task
                        TAG.data(),         // task name
                        desk::stack.size(), // stack size
                        nullptr,            // task data (use ptr() to access LightDesk)
                        5,                  // priority
                        desk::stack.data(), // static task stack
                        &desk::tcb          // task control block
      );

  ESP_LOGI(TAG.data(), "started tcb=%p", &desk::tcb);
  return shared_from_this();
}

// IRAM_ATTR void LightDesk::messageLoop() {
//   [[maybe_unused]] static constexpr csv fn_id = "messageLoop()";
//
//   socket.async_receive_from(asio::buffer(rx_buff), remote_endpoint,
//                             [this, &buff = rx_buff](error_code ec, size_t rx_bytes) {
//                               // success and actual bytes
//                               if (!ec && rx_bytes) {
//                                 ESP_LOGD(TAG.data(), "%s rx=%04d remote=%s", fn_id.data(),
//                                          rx_bytes,
//                                          remote_endpoint.address().to_string().c_str());
//
//                                 auto msg = DeskMsg(buff, rx_bytes);
//
//                                 if (msg.validMagic()) {
//                                   DMX::handleFrame(msg.dframe<DMX::Frame>());
//
//                                   // for (auto unit : desk::units) {
//                                   //   unit->handleMsg(msg.root());
//                                   // }
//
//                                   idleWatchDog(); // reset watchdog, we have a msg
//                                 } else {
//                                   ESP_LOGD(TAG.data(), "%s bad magic", fn_id.data());
//                                 }
//                               } else {
//                                 ESP_LOGW(TAG.data(), "%s ec=%s\n", fn_id.data(),
//                                          ec.message().c_str());
//                               }
//
//                               messageLoop();
//                             });
// }

// definefd in .cpp to limit exposure of Advertise
void LightDesk::run() {

  auto server = //
      std::unique_ptr<desk::Server>(new desk::Server({.io_ctx = io_ctx,
                                                      .listen_port = SERVICE_PORT,
                                                      .idle_shutdown = opts.idle_shutdown,
                                                      .idle_check = opts.idle_check}));

  desk::Advertise::create(server->localPort())->init();

  // idleWatchDog();      // schedule idle watch dog
  server->asyncLoop(); // schedule accept connections
  // messageLoop();

  // const auto msg_port = socket.local_endpoint().port();
  // desk::Advertise::create(server->localPort())->init()->addService();

  io_ctx.run(); // returns when all io_ctx work is complete

  ESP_LOGI(TAG.data(), "run() io_ctx work exhausted");

  state.zombie();
}

} // namespace ruth
