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

#include "dev_pwm/pwm.hpp"
#include "engine_pwm/pwm.hpp"
#include "mqtt.hpp"

namespace pwm {

using namespace std::string_view_literals;
using namespace ruth;

static Engine *_instance_ = nullptr;

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

const size_t _capacity =
    JSON_ARRAY_SIZE(8) + 8 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 220;

Engine::Engine() : Handler(5), _known({Device(1), Device(2), Device(3), Device(4)}) {
  device::PulseWidth::allOff(); // ensure all pins are off at initialization

  // addTask(TASK_CORE);
  // addTask(TASK_REPORT);
}

//
// Commands (processed via Core Task)
//

// void Engine::commandLocal(MsgPayload_t_ptr payload) {
//   // deserialize the msgpack data
//   StaticJsonDocument<_capacity> doc;
//   DeserializationError err = deserializeMsgPack(doc, payload.get()->payload());
//
//   // NOTE
//   //      The original payload MUST be kept until we are completely finished
//   //      with the JsonDocument
//
//   // did the deserailization succeed?
//   if (err) {
//     Text::rlog("pwm command MSGPACK err=\"%s\"", err.c_str());
//     return;
//   }
//
//   commandExecute(doc);
// }

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

void Engine::command(void *task_data) {

  // core task run loop
  //  1.  acts on task notifications
  //  2.  acts on queued command messages

  for (;;) {

    auto msg = waitForMessage();
  }
}

void Engine::report(void *data) {
  static TickType_t last_wake;

  for (;;) {
    last_wake = xTaskGetTickCount();

    for (size_t i = 0; i < _num_devices; i++) {
      // auto &device = _known[i];
      //
      // auto duty = device.duty();
    }

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(7000));
  }
}

void Engine::start(Opts &opts) { MQTT::registerHandler(this, "pwm"sv); }

void Engine::wantMessage(message::InWrapped &msg) {}

} // namespace pwm
