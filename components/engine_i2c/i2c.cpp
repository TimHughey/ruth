/*
      Ruth
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

#include "dev_i2c/i2c.hpp"
#include "dev_i2c/mcp23008.hpp"
#include "dev_i2c/sht31.hpp"
#include "engine_i2c/i2c.hpp"
#include "ruth_mqtt/mqtt.hpp"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ruth {
namespace i2c {

static const char *TAG_RPT = "i2c:report";
static const char *TAG_CMD = "i2c:cmd";
static const uint8_t discover_addresses[] = {0x44, 0x20};
static Engine *_instance_ = nullptr;
static TickType_t last_wake;

Engine::Engine(const Opts &opts) : Handler("i2c", max_queue_depth), _opts(opts) {
  Device::setUniqueId(opts.unique_id);

  // create the devices we support
  _devices[0] = new MCP23008();
  _devices[1] = new SHT31();
}

IRAM_ATTR void Engine::command(void *task_data) {
  Engine *i2c = (Engine *)task_data;

  i2c->notifyThisTask(Notifies::QUEUED_MSG);
  MQTT::registerHandler(i2c);

  for (;;) {
    UBaseType_t notify_val;
    auto msg = i2c->waitForNotifyOrMessage(&notify_val);

    if (msg) {
      // only look at mutable devices. since we only support mcp23008
      // the first mutable found is the one we want.
      for (auto i = 0; i < device_count; i++) {
        auto device = i2c->devices(i);

        if (device->isMutable()) {
          device->execute(std::move(msg));
          break;
        }
      }
    }
  }
}

IRAM_ATTR void Engine::report(void *data) {
  Engine *i2c = (Engine *)data;
  const auto send_ms = i2c->_opts.report.send_ms;

  Device::initHardware();

  for (;;) {
    last_wake = xTaskGetTickCount();

    for (auto i = 0; i < device_count; i++) {
      auto device = i2c->devices(i);
      device->report();
    }

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(send_ms));
  }
}

void Engine::start(const Opts &opts) {
  if (_instance_)
    return;

  _instance_ = new Engine(opts);

  TaskHandle_t &report_task = _instance_->_tasks[REPORT];

  xTaskCreate(&report, TAG_RPT, opts.report.stack, _instance_, opts.report.priority, &report_task);

  TaskHandle_t &cmd_task = _instance_->_tasks[COMMAND];
  xTaskCreate(&command, TAG_CMD, opts.command.stack, _instance_, opts.command.priority, &cmd_task);
}

void Engine::wantMessage(message::InWrapped &msg) { msg->want(DocKinds::CMD); }

} // namespace i2c
} // namespace ruth
