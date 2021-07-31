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

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "dev_i2c/i2c.hpp"
#include "dev_i2c/mcp23008.hpp"
#include "dev_i2c/sht31.hpp"
#include "engine_i2c/i2c.hpp"
#include "ruth_mqtt/mqtt.hpp"

namespace i2c {
using namespace ruth;

static const char *TAG_RPT = "i2c:report";
static const char *TAG_CMD = "i2c:cmd";
static const uint8_t discover_addresses[] = {0x44, 0x20};
static Engine *_instance_ = nullptr;

Engine::Engine(const Opts &opts) : Handler("i2c", max_queue_depth), _opts(opts) {
  Device::setUniqueId(opts.unique_id);
}

IRAM_ATTR void Engine::command(void *task_data) {
  Engine *ds = (Engine *)task_data;

  ds->notifyThisTask(Notifies::QUEUED_MSG);
  MQTT::registerHandler(ds);

  ESP_LOGD(TAG_CMD, "task started");

  for (;;) {
    UBaseType_t notify_val;
    auto msg = ds->waitForNotifyOrMessage(&notify_val);

    if (msg) {
      const char *ident = msg->filterExtra(0);

      Device *cmd_device = ds->findDevice(ident);

      if (cmd_device) {
        cmd_device->execute(std::move(msg));
      }

    } else {
      ESP_LOGW(TAG_CMD, "unhandled notify: 0x%x", notify_val);
    }
  }
}

IRAM_ATTR void Engine::discover(const uint32_t loops_per_discover) {
  static uint32_t loop_count = 0;

  // don't discover until enough loops have passed.  by using a countdown we are assured the
  // first call will always perform a discover.
  if (loop_count > 0) {
    loop_count--;
    return;
  } else {
    // it is time for a discover, reset the loop count and proceed with the discover
    loop_count = loops_per_discover;
  }

  for (size_t i = 0; i < sizeof(discover_addresses); i++) {
    if (_known[i] == nullptr) {
      Device *to_detect = nullptr;
      Device *detected = nullptr;

      auto find_addr = discover_addresses[i];

      switch (find_addr) {
      case 0x20:
        to_detect = new MCP23008();
        if (to_detect->detect()) detected = to_detect;
        break;

      case 0x44:
        to_detect = new SHT31();
        if (to_detect->detect()) detected = to_detect;
        break;
      }

      if (detected) {
        _known[i] = detected;
      } else {
        delete to_detect;
      }
    }
  }
}

IRAM_ATTR Device *Engine::findDevice(const char *ident) {
  Device *found = nullptr;

  for (size_t i = 0; i < max_devices; i++) {
    Device *device = _known[i];

    if (device == nullptr) continue;

    if (strncmp(device->ident(), ident, Device::identMaxLen()) == 0) {
      found = device;
      break;
    }
  }

  return found;
}

IRAM_ATTR void Engine::report(void *data) {
  static TickType_t last_wake;

  Engine *ds = (Engine *)data;
  const auto send_ms = ds->_opts.report.send_ms;
  const auto loops_per_discover = ds->_opts.report.loops_per_discover;

  Device::initHardware();

  ESP_LOGD(TAG_RPT, "task started");

  for (;;) {
    last_wake = xTaskGetTickCount();

    // important to discover first especially at startup
    ds->discover(loops_per_discover);

    for (size_t i = 0; i < max_devices; i++) {
      Device *device = ds->_known[i];

      if (device == nullptr) continue; // reached the end of known devices

      device->report();
    }

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(send_ms));
  }
}

void Engine::start(const Opts &opts) {
  if (_instance_) return;

  _instance_ = new Engine(opts);

  TaskHandle_t &report_task = _instance_->_tasks[REPORT];

  xTaskCreate(&report, TAG_RPT, opts.report.stack, _instance_, opts.report.priority, &report_task);

  TaskHandle_t &cmd_task = _instance_->_tasks[COMMAND];
  xTaskCreate(&command, TAG_CMD, opts.command.stack, _instance_, opts.command.priority, &cmd_task);
}

void Engine::wantMessage(message::InWrapped &msg) { msg->want(DocKinds::CMD); }

} // namespace i2c
