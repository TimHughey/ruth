/*
      pwm_engine.cpp - Ruth PWM Engine
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

#include <string_view>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ack_msg.hpp"
#include "dev_pwm/pwm.hpp"
#include "engine_pwm/pwm.hpp"
#include "misc/status_led.hpp"
#include "mqtt.hpp"
#include "status_msg.hpp"

namespace pwm {

using namespace std::string_view_literals;
using namespace ruth;

static const char *TAG_RPT = "pwm:report";
static const char *TAG_CMD = "pwm:cmd";
static Engine *_instance_ = nullptr;
char Engine::_ident[32] = {};

// document example:
// {"ack":true,
//   "device":"pwm/lab-ledge.pin:1","host":"ruth.3c71bf14fdf0",
//   "refid":"3b952cbb-324c-4e98-8fd5-3f484f41c975",
//   "seq":{"name":"flash","repeat":true,"run":true,
//     "steps":
//       [{"duty":8191,"ms":750},{"duty":0,"ms":1500},
//        {"duty":4096,"ms":750},{"duty":0,"ms":1500},
//        {"duty":2048,"ms":750},{"duty":0,"ms":1500},
//        {"duty":1024,"ms":750},{"duty":0,"ms":1500}]}}

// const size_t _capacity =
//     JSON_ARRAY_SIZE(8) + 8 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 220;

Engine::Engine(const char *unique_id, uint32_t report_send_ms)
    : Handler(5), _known({Device(1), Device(2), Device(3), Device(4)}), _report_send_ms(report_send_ms) {

  constexpr size_t capacity = sizeof(_ident) - 1;
  auto *p = _ident;

  // very efficiently populate the prefix
  *p++ = 'p';
  *p++ = 'w';
  *p++ = 'm';

  memccpy(p, unique_id, 0x00, capacity - (p - _ident));
}

// bool Engine::commandExecute(JsonDocument &doc) {
//   PwmDevice_t *dev = findDevice(doc["device"]);
//   auto set_rc = false;
//
//   if (dev && dev->valid()) {
//     const uint32_t pwm_cmd = doc["pwm_cmd"] | 0x00;
//
//     if (pwm_cmd < 0x1000) {
//       // this command is device specific, send it there for processing
//       set_rc = dev->cmd(pwm_cmd, doc);
//     } else {
//       // this command is for the engine
//       set_rc = true;
//     }
//
//     bool ack = doc["ack"] | true;
//     const RefID_t refid = doc["refid"].as<const char *>();
//     commandAck(dev, ack, refid, set_rc);
//   }
//
//   return set_rc;
// }

//
// Tasks
//

static StaticJsonDocument<1024> cmd_doc;

void Engine::command(void *task_data) {
  Engine *pwm = (Engine *)task_data;
  MQTT::registerHandler(pwm, "pwm"sv);

  ESP_LOGI(TAG_CMD, "task started");

  for (;;) {
    auto msg = pwm->waitForMessage();

    if (msg) {
      if (msg->unpack(cmd_doc)) {
        const char *refid = msg->filter(4);
        const char *cmd = cmd_doc["cmd"];
        const char *type = cmd_doc["type"];
        bool ack = false;

        const uint8_t pin = cmd_doc["pin"].as<uint8_t>();
        Device &dev = (pin == 0) ? StatusLED::device() : pwm->_known[pin - 1];

        if (type) {
          ESP_LOGI(TAG_CMD, "custom command[%s] type[%s]", cmd, type);
        } else {
          ack = dev.execute(cmd);
        }

        if (ack) {
          pwm::Ack ack_msg(refid);

          MQTT::send(ack_msg);
        }
      }
    }
  }
}

void Engine::report(void *data) {
  static TickType_t last_wake;

  Engine *pwm = (Engine *)data;
  const auto send_ms = pwm->_report_send_ms;

  ESP_LOGI(TAG_RPT, "task started: send_ms[%u]", send_ms);

  for (;;) {
    last_wake = xTaskGetTickCount();
    {
      pwm::Status status(pwm->_ident);

      auto &status_led = StatusLED::device();
      status.addPin(status_led.pinNum(), status_led.status());

      for (size_t i = 0; i < _num_devices; i++) {
        auto &device = pwm->_known[i];
        device.makeStatus();

        status.addPin(device.pinNum(), device.status());
      }

      MQTT::send(status);
    }

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(7000));
  }
}

void Engine::start(Opts &opts) {
  if (_instance_) return;

  _instance_ = new Engine(opts.unique_id, opts.report.send_ms);
  TaskHandle_t &report_task = _instance_->_report_task;

  xTaskCreate(&report, TAG_RPT, opts.report.stack, _instance_, opts.report.priority, &report_task);

  TaskHandle_t &cmd_task = _instance_->_command_task;
  xTaskCreate(&command, TAG_CMD, opts.command.stack, _instance_, opts.command.priority, &cmd_task);
}

void Engine::wantMessage(message::InWrapped &msg) {
  const char *ident = msg->filter(3);

  if (strncmp(_ident, ident, sizeof(_ident)) == 0) {
    msg->want(DocKinds::CMD);
  }
}

} // namespace pwm
