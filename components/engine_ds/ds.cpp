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

// #include "ack_msg.hpp"
#include "dev_ds/ds.hpp"
#include "dev_ds/ds1820.hpp"
#include "engine_ds/ds.hpp"
#include "ruth_mqtt/mqtt.hpp"

namespace ds {
using namespace ruth;

static const char *TAG_RPT = "ds:report";
static const char *TAG_CMD = "ds:cmd";
static Engine *_instance_ = nullptr;

Engine::Engine(const Opts &opts) : Handler("ds", max_queue_depth), _opts(opts) {}

static StaticJsonDocument<1024> cmd_doc;

void Engine::command(void *task_data) {
  Engine *ds = (Engine *)task_data;

  ds->notifyThisTask(Notifies::QUEUED_MSG);
  MQTT::registerHandler(ds);

  // ESP_LOGI(TAG_CMD, "task started");

  for (;;) {
    // UBaseType_t notify_val;
    // auto msg = ds->waitForNotifyOrMessage(&notify_val);
    //
    // if (msg) {
    //   if (msg->unpack(cmd_doc)) {
    //     const char *refid = msg->filter(4);
    //     // const char *cmd = cmd_doc["cmd"];
    //     // const char *type = cmd_doc["type"];
    //     const JsonObject root = cmd_doc.as<JsonObject>();
    //
    //     const bool ack = root["ack"] | false;
    //
    //     // auto execute_rc = dev.execute(root);
    //     auto execute_rc = true;
    //
    //     if (ack && execute_rc) {
    //       ds::Ack ack_msg(refid);
    //
    //       MQTT::send(ack_msg);
    //     }
    //   }
    // } else {
    //   ESP_LOGW(TAG_CMD, "unhandled notify: 0x%x", notify_val);
    // }

    vTaskDelay(portMAX_DELAY);
  }
}

void Engine::discover() {

  uint8_t rom_code[8];
  bool found;
  do {
    found = Device::search(rom_code);

    if (found) {
      for (size_t i = 0; i < max_devices; i++) {
        constexpr size_t rom_len = sizeof(rom_code);
        Device *entry = _known[i];

        // we've reached the end of the known devices and the rom code isn't known.
        // add the rom code as a known device and get out of this loop
        if (entry == nullptr) {
          Device *new_device = nullptr;

          const uint8_t family = rom_code[0];
          switch (family) {
          case 0x28:
            new_device = new DS1820(rom_code);
            break;
          }

          if (new_device) {
            ESP_LOGI(new_device->ident(), "new device");
            _known[i] = new_device;
          }

          break;
        }

        if (memcmp(entry->addr(), rom_code, rom_len) == 0) {
          // we already know this device
          auto diff_us = entry->updateSeenTimestamp();

          ESP_LOGD(entry->ident(), "previously seen %u µs ago", diff_us);

          break;
        }
      }
    }

  } while (found);
}

void Engine::report(void *data) {
  static TickType_t last_wake;
  static uint32_t discover_loop_count = 0;

  Engine *ds = (Engine *)data;
  const auto send_ms = ds->_opts.report.send_ms;
  Device::initHardware(send_ms);

  // ESP_LOGI(TAG_RPT, "task started: send_ms[%u]", send_ms);

  for (;;) {
    last_wake = xTaskGetTickCount();
    // {
    //   ds::Status status(ds->_ident);
    //
    //   auto &status_led = StatusLED::device();
    //   status_led.makeStatus();
    //   status.addPin(status_led.pinNum(), status_led.status());
    //
    //   for (size_t i = 0; i < _num_devices; i++) {
    //     auto &device = ds->_known[i];
    //     device.makeStatus();
    //
    //     status.addPin(device.pinNum(), device.status());
    //   }
    //
    //   MQTT::send(status);
    // }

    if (discover_loop_count == 0) {
      discover_loop_count = ds->_opts.report.loops_per_discover;
      ds->discover();
    } else {
      discover_loop_count--;
    }

    for (size_t i = 0; i < max_devices; i++) {
      Device *device = ds->_known[i];

      if (device == nullptr) break;

      if (device->report()) {
        // ESP_LOGI(device->ident(), "celsius: %2.2f", temp_c);
      }
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

void Engine::wantMessage(message::InWrapped &msg) {
  // const char *ident = msg->filter(3);

  // if (strncmp(_ident, ident, sizeof(_ident)) == 0) {
  msg->want(DocKinds::CMD);
  // }
}

} // namespace ds
