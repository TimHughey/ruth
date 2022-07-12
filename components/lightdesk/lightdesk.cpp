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
#include "headunit/ac_power.hpp"
#include "headunit/discoball.hpp"
#include "headunit/elwire.hpp"
#include "headunit/headunit.hpp"
#include "headunit/ledforest.hpp"
#include "io/io.hpp"
#include "msg.hpp"

#include <array>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ruth {
namespace shared {
shLightDesk __lightdesk;
shLightDesk lightdesk() { return __lightdesk; }

static HeadUnits headunits;
TaskHandle_t lightdesk_task;
} // namespace shared

namespace desk {
DRAM_ATTR static StaticTask_t tcb;
DRAM_ATTR static std::array<StackType_t, 4096> stack;
DRAM_ATTR static HeadUnits units;

} // namespace desk

static const char *TAG = "lightdesk";
constexpr size_t RX_MAX_LEN = 1024;
DRAM_ATTR static std::array<char, RX_MAX_LEN> rx_buff;
// DRAM_ATTR static DeskMsg desk_msg;

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

// static functions for FreeRTOS task
static void task_start([[maybe_unused]] void *data) { LightDesk::ptr()->run(); }

// general API

IRAM_ATTR void LightDesk::idleWatchDog() {
  // each call will restart the timer
  idle_timer.expires_after(idle_check);

  idle_timer.async_wait( //
      [self = shared_from_this(), &units = shared::headunits](error_code ec) {
        // if the timer ever expires then we're idle
        if (!ec) {
          std::for_each(units.begin(), units.end(), [](shHeadUnit unit) { unit->dark(); });

          self->state = desk::State::IDLE;

          self->idleWatchDog(); // always reschedule self
        }
      });
}

shLightDesk LightDesk::init() {
  ESP_LOGD(TAG, "enabled, starting up");

  shared::headunits.emplace_back(std::make_shared<AcPower>());
  shared::headunits.emplace_back(std::make_shared<DiscoBall>(1));     // pwm 1
  shared::headunits.emplace_back(std::make_shared<ElWire>("EL1", 2)); // pwm 2
  shared::headunits.emplace_back(std::make_shared<ElWire>("EL2", 3)); // pwm 3
  shared::headunits.emplace_back(std::make_shared<LedForest>(4));     // pwm 4

  shared::lightdesk_task =                  // create the task using a static stack
      xTaskCreateStatic(&task_start,        // static func to start task
                        TAG,                // task name
                        desk::stack.size(), // stack size
                        nullptr,            // task data (use ptr() to access LightDesk)
                        13,                 // priority
                        desk::stack.data(), // static task stack
                        &desk::tcb          // task control block
      );

  return shared_from_this();
}

IRAM_ATTR void LightDesk::messageLoop() {

  socket.async_receive_from(asio::buffer(rx_buff), remote_endpoint,
                            [this, &buff = rx_buff](error_code ec, size_t rx_bytes) {
                              if (!ec && rx_bytes) {
                                if (auto msg = DeskMsg(buff); msg.validMagic()) {

                                  idleWatchDog(); // reset watchdog, we have a msg
                                }
                              }
                            });
}

IRAM_ATTR void LightDesk::run() {
  idleWatchDog();
  messageLoop();

  io_ctx.run(); // returns when all io_ctx work is complete

  state = desk::State::ZOMBIE;
}

} // namespace ruth
