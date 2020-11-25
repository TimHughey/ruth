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

#include "engines/pwm.hpp"

namespace ruth {
using std::unique_ptr;
using namespace reading;

static PulseWidth_t *__singleton__ = nullptr;

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

const size_t _capacity = JSON_ARRAY_SIZE(8) + 8 * JSON_OBJECT_SIZE(2) +
                         JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 220;

PulseWidth::PulseWidth() : Engine() {
  configureTimer();
  PwmDevice::allOff(); // ensure all pins are off at initialization

  addTask(TASK_CORE);
  addTask(TASK_REPORT);
}

// STATIC!
bool PulseWidth::engineEnabled() { return (__singleton__) ? true : false; }

//
// Commands (processed via Core Task)
//

void PulseWidth::commandLocal(MsgPayload_t_ptr payload) {
  // deserialize the msgpack data
  StaticJsonDocument<_capacity> doc;
  DeserializationError err = deserializeMsgPack(doc, payload.get()->payload());

  // NOTE
  //      The original payload MUST be kept until we are completely finished
  //      with the JsonDocument

  // did the deserailization succeed?
  if (err) {
    Text::rlog("pwm command MSGPACK err=\"%s\"", err.c_str());
    return;
  }

  commandExecute(doc);
}

bool PulseWidth::commandExecute(JsonDocument &doc) {
  PwmDevice_t *dev = findDevice(doc["device"]);
  auto set_rc = false;

  if (dev && dev->valid()) {
    const uint32_t pwm_cmd = doc["pwm_cmd"] | 0x00;

    if (pwm_cmd < 0x1000) {
      // this command is device specific, send it there for processing
      set_rc = dev->cmd(pwm_cmd, doc);
    } else {
      // this command is for the engine
      set_rc = true;
    }

    bool ack = doc["ack"] | true;
    const RefID_t refid = doc["refid"].as<const char *>();
    commandAck(dev, ack, refid, set_rc);
  }

  return set_rc;
}

//
// Tasks
//

void PulseWidth::core(void *task_data) {
  // create the command queue
  _cmd_q = xQueueCreate(_max_queue_depth, sizeof(MsgPayload_t *));

  Net::waitForNormalOps();

  // discovering the pwm devices is ultimately creating and adding them
  // to the known device list since they are onboard hardware.
  for (uint8_t i = 1; i <= 4; i++) {
    DeviceAddress_t addr(i);
    PwmDevice_t dev(addr);

    PwmDevice *new_dev = new PwmDevice(dev);
    unique_ptr<PwmDevice> new_dev_ptr(new_dev);

    new_dev->configureChannel();

    if (new_dev->lastRC() == ESP_OK) {
      // release the pointer as we pass it to the known device list
      new_dev_ptr.release();
      addDevice(new_dev);
    }
  }

  notifyDevicesAvailable();
  holdForDevicesAvailable();

  // core task run loop
  //  1.  acts on task notifications
  //  2.  acts on queued command messages

  for (;;) {
    MsgPayload_t *payload = nullptr;

    // wait for a queued command
    // if one is available, wrap it in a unique ptr then send it to
    // commandLocal for processing
    auto queue_rc = xQueueReceive(_cmd_q, &payload, pdMS_TO_TICKS(1000));

    if (queue_rc == pdTRUE) {
      unique_ptr<MsgPayload_t> payload_ptr(payload);
      commandLocal(move(payload_ptr));
    }

    auto notify_val = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));

    if (notify_val) {
      auto stack_hw = uxTaskGetStackHighWaterMark(nullptr);

      Text::rlog("core task notified, stack_hw=%d", stack_hw);
    }
  }
}

void PulseWidth::report(void *data) {
  static TickType_t last_wake;

  holdForDevicesAvailable();
  saveLastWake(last_wake);

  for (;;) {
    Net::waitForNormalOps();

    if (numKnownDevices() == 0) {
      delayUntil(last_wake, reportFrequency());
      continue;
    }

    for_each(deviceMap().begin(), deviceMap().end(), [this](PwmDevice_t *dev) {
      if (dev->available()) {
        if (readDevice(dev)) {
          publish(dev);
        }
      }
    });

    delayUntil(last_wake, reportFrequency());
  }
}

void PulseWidth::configureTimer() {
  esp_err_t timer_rc;

  ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_HIGH_SPEED_MODE,
                                    .duty_resolution = LEDC_TIMER_13_BIT,
                                    .timer_num = LEDC_TIMER_1,
                                    .freq_hz = 5000,
                                    .clk_cfg = LEDC_AUTO_CLK};

  timer_rc = ledc_timer_config(&ledc_timer);

  if (timer_rc != ESP_OK) {
    Text::rlog("ledc timer config error=\"%s\"", esp_err_to_name(timer_rc));
  }
}

PulseWidth_t *PulseWidth::_instance_() {
  if (__singleton__ == nullptr) {
    __singleton__ = new PulseWidth();
  }

  return __singleton__;
}

bool PulseWidth::readDevice(PwmDevice_t *dev) {
  auto rc = false;

  dev->readStart();
  auto duty = ledc_get_duty(dev->speedMode(), dev->channel());
  dev->readStop();

  if (duty != LEDC_ERR_DUTY) {
    Reading_t *reading =
        new Pwm(dev->id(), dev->dutyMax(), dev->dutyMin(), duty);

    reading->setLogReading();
    dev->setReading(reading);
    rc = true;
  }

  return rc;
}

} // namespace ruth
