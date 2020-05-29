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

using std::unique_ptr;

namespace ruth {

static PulseWidth_t *__singleton__ = nullptr;
static const string_t engine_name = "PWM";

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

// const size_t _capacity = JSON_ARRAY_SIZE(8) + 8 * JSON_OBJECT_SIZE(2) +
//                          JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 220;

const size_t _capacity = 5 * 1024;

PulseWidth::PulseWidth() : Engine(ENGINE_PWM) {
  pwmDev::allOff(); // ensure all pins are off at initialization

  addTask(TASK_CORE);
  addTask(TASK_REPORT);
  addTask(TASK_COMMAND);
}

// STATIC!
bool PulseWidth::engineEnabled() { return (__singleton__) ? true : false; }

//
// Tasks
//

void PulseWidth::command(void *data) {
  _cmd_q = xQueueCreate(_max_queue_depth, sizeof(MsgPayload_t *));

  while (true) {
    BaseType_t queue_rc = pdFALSE;
    MsgPayload_t *payload = nullptr;

    auto notify_val = ulTaskNotifyTake(pdTRUE, 0);

    if (notify_val) {
      ESP_LOGI(engine_name.c_str(), "command task notified: %d", notify_val);
    }

    _cmd_elapsed.reset();

    queue_rc = xQueueReceive(_cmd_q, &payload, portMAX_DELAY);
    // wrap in a unique_ptr so it is freed when out of scope
    unique_ptr<MsgPayload_t> payload_ptr(payload);

    if (queue_rc == pdFALSE) {
      continue;
    }

    // deserialize the msgpack data
    DynamicJsonDocument doc(_capacity);
    DeserializationError err = deserializeMsgPack(doc, payload->payload());

    //
    // NOTE
    //
    // The original payload MUST be kept until we are completely finished
    // with the JsonDocument
    //

    // did the deserailization succeed?
    if (err) {
      ST::rlog("pwm command MSGPACK err=\"%s\"", err.c_str());
      continue;
    }

    commandExecute(doc);
  }
}

bool PulseWidth::commandExecute(JsonDocument &doc) {
  pwmDev_t *dev = findDevice(doc["device"]);
  auto set_rc = false;

  if (dev == nullptr) {
    return false;
  }

  if (dev->notValid()) {
    return false;
  }

  const uint32_t pwm_cmd = doc["pwm_cmd"] | 0x00;

  switch (pwm_cmd) {

  case 0x10: // duty
    set_rc = dev->updateDuty(doc);
    break;

  case 0x20:
    set_rc = dev->sequence(doc);
    break;

  default:
    set_rc = false;
    break;
  }

  bool ack = doc["ack"] | true;
  const string_t refid = doc["refid"];
  commandAck(dev, ack, refid, set_rc);

  return set_rc;
}

void PulseWidth::core(void *task_data) {
  if (configureTimer() == false) {
    return;
  }

  Net::waitForNormalOps();

  // NOTE:
  //   As of 2020-05-18 engines are started after name assignment is complete
  //   so the following line is not necessary
  // net_name = Net::waitForName();

  // signal to other tasks the dsEngine task is in it's run loop
  // this ensures other tasks wait until core setup is complete

  saveTaskLastWake(TASK_CORE);

  // discovering the pwm devices is ultimately creating and adding them
  // to the known device list since they are onboard hardware.
  for (uint8_t i = 1; i <= 4; i++) {
    DeviceAddress_t addr(i);
    pwmDev_t dev(addr);

    unique_ptr<pwmDev> new_dev_ptr(new pwmDev(dev));

    new_dev_ptr.get()->setMissingSeconds(60);
    new_dev_ptr.get()->configureChannel();

    if (new_dev_ptr.get()->lastRC() == ESP_OK) {
      // release the pointer as we pass it to the known device list
      addDevice(new_dev_ptr.get());
      new_dev_ptr.release();
    }
  }

  engineRunning();

  if (numKnownDevices() > 0) {
    devicesAvailable();
  }

  // task run loop
  for (;;) {
    // as of now there is nothing for this loop to do so we'll use
    // task notification to wake up (if needed)
    auto notify_val = ulTaskNotifyTake(pdTRUE, UINT32_MAX);

    if (notify_val) {
      ESP_LOGI(engine_name.c_str(), "core task notified: %d", notify_val);
    }
  }
}

void PulseWidth::report(void *data) {
  saveTaskLastWake(TASK_REPORT);

  while (waitFor(devicesAvailableBit())) {
    if (numKnownDevices() == 0) {
      taskDelayUntil(TASK_REPORT, _report_frequency);
      continue;
    }

    Net::waitForNormalOps();

    for_each(beginDevices(), endDevices(), [this](pwmDev_t *item) {
      pwmDev_t *dev = item;

      if (dev->available()) {

        if (readDevice(dev)) {
          publish(dev);
        }
      }
    });

    taskDelayUntil(TASK_REPORT, _report_frequency);
  }
}

bool PulseWidth::configureTimer() {
  esp_err_t timer_rc;

  ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_HIGH_SPEED_MODE,
                                    .duty_resolution = LEDC_TIMER_13_BIT,
                                    .timer_num = LEDC_TIMER_1,
                                    .freq_hz = 5000,
                                    .clk_cfg = LEDC_AUTO_CLK};

  timer_rc = ledc_timer_config(&ledc_timer);

  if (timer_rc == ESP_OK) {
    ESP_LOGD(engine_name.c_str(), "ledc timer configured");
    return true;
  } else {
    ESP_LOGE(engine_name.c_str(), "ledc timer [%s]", esp_err_to_name(timer_rc));
    return false;
  }
}

PulseWidth_t *PulseWidth::_instance_() {
  if (__singleton__ == nullptr) {
    __singleton__ = new PulseWidth();
  }

  return __singleton__;
}

bool PulseWidth::readDevice(pwmDev_t *dev) {
  auto rc = false;

  dev->readStart();
  auto duty = ledc_get_duty(dev->speedMode(), dev->channel());
  dev->readStop();

  if (duty == LEDC_ERR_DUTY) {
    ESP_LOGW(engine_name.c_str(), "error reading duty");
  } else {
    pwmReading_t *reading = new pwmReading(
        dev->id(), time(nullptr), dev->dutyMax(), dev->dutyMin(), duty);

    reading->setLogReading();
    dev->setReading(reading);
    rc = true;
  }

  return rc;
}

} // namespace ruth
