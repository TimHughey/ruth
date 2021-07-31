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

#include "dev_ds/ds.hpp"
#include "dev_ds/ds1820.hpp"
#include "dev_ds/ds2408.hpp"
#include "engine_ds/ds.hpp"
#include "ruth_mqtt/mqtt.hpp"

namespace ds {
using namespace ruth;

static const char *TAG_RPT = "ds:report";
static const char *TAG_CMD = "ds:cmd";
static Engine *_instance_ = nullptr;

Engine::Engine(const Opts &opts) : Handler("ds", max_queue_depth), _opts(opts) {}

IRAM_ATTR void Engine::command(void *task_data) {
  Engine *ds = (Engine *)task_data;

  ds->notifyThisTask(Notifies::QUEUED_MSG);
  MQTT::registerHandler(ds);

  ESP_LOGD(TAG_CMD, "task started");

  for (;;) {
    UBaseType_t notify_val;
    auto msg = ds->waitForNotifyOrMessage(&notify_val);

    if (msg) {
      const char *ident = msg->identFromFilter();

      Device *cmd_device = ds->findDevice(ident);

      if (cmd_device) {
        Device::acquireBus();
        cmd_device->execute(std::move(msg));
        Device::releaseBus();
      }

    } else {
      ESP_LOGW(TAG_CMD, "unhandled notify: 0x%x", notify_val);
    }
  }
}

IRAM_ATTR void Engine::discover(const uint32_t loops_per_discover) {
  static uint32_t loop_count = 0;

  // don't discover until enough loops have passed.
  // using a countdown we are assured the first call will always perform a discover.
  if (loop_count == 0) {
    // it is time for a discover, reset the loop count and proceed with the discover
    loop_count = loops_per_discover;
  } else {
    loop_count--;
    return;
  }

  uint8_t rom_code[8];
  bool found;
  uint32_t found_count = 0;

  do {
    found = Device::search(rom_code);

    if (found) {
      found_count++;

      for (size_t i = 0; i < max_devices; i++) {
        // constexpr size_t rom_len = sizeof(rom_code);
        Device *entry = _known[i];

        // we've reached the end of the known devices and the rom code isn't known.
        // add the rom code as a known device and get out of this loop
        if (entry == nullptr) {
          Device *new_device = nullptr; // we never delete a device so no reason to use smart pointer

          const uint8_t family = rom_code[0];
          switch (family) {
          case 0x28:
            new_device = new DS1820(rom_code);
            break;

          case 0x29:
            new_device = new DS2408(rom_code);
            break;

          default:
            ESP_LOGW(TAG_RPT, "unhandled family: 0x%02x", family);
            break;
          }

          if (new_device) {
            ESP_LOGD(new_device->ident(), "new device");
            _known[i] = new_device;
          }

          break;
        }

        // if (memcmp(entry->addr(), rom_code, rom_len) == 0) {
        //   // we already know this device
        //   auto diff_us = entry->updateSeenTimestamp();
        //
        //   ESP_LOGD(entry->ident(), "previously seen %u µs ago", diff_us);
        //
        //   break;
        // }
      }
    }

  } while (found);

  ESP_LOGD(TAG_RPT, "discovered %d devices", found_count);
}

IRAM_ATTR Device *Engine::findDevice(const char *ident) {
  Device *found = nullptr;

  for (auto i = 0; i < max_devices; i++) {
    Device *device = _known[i];

    if (device == nullptr) {
      break;
    }

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

  Device::initHardware(send_ms);

  ESP_LOGD(TAG_RPT, "task started");

  for (;;) {
    last_wake = xTaskGetTickCount();
    if (Device::acquireBus(1000)) {
      // important to discover first especially at startup
      ds->discover(loops_per_discover);

      for (size_t i = 0; i < max_devices; i++) {
        Device *device = ds->_known[i];

        if (device == nullptr) break; // reached the end of known devices

        device->report();
      }

      Device::releaseBus();

    } else {
      ESP_LOGW(TAG_RPT, "timeout acquiring bus");
    }

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(send_ms));
  }
}

void Engine::start(const Opts &opts) {
  if (_instance_) return;

  _instance_ = new Engine(opts);

  // pass the send frequency to Hardware for convert frequency calculations

  TaskHandle_t &report_task = _instance_->_tasks[REPORT];

  xTaskCreate(&report, TAG_RPT, opts.report.stack, _instance_, opts.report.priority, &report_task);

  TaskHandle_t &cmd_task = _instance_->_tasks[COMMAND];
  xTaskCreate(&command, TAG_CMD, opts.command.stack, _instance_, opts.command.priority, &cmd_task);
}

void Engine::wantMessage(message::InWrapped &msg) { msg->want(DocKinds::CMD); }

} // namespace ds
