/*
      i2c_engine.cpp - Ruth I2C
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

// #define VERBOSE 1

#include <cstdlib>
#include <string>

#include <driver/i2c.h>
#include <driver/periph_ctrl.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "engines/i2c.hpp"

namespace ruth {

static I2c_t *__singleton__ = nullptr;

// command document capacity for expected metadata and up to eight pio states
const size_t _capacity =
    JSON_ARRAY_SIZE(8) + 8 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(7) + 227;

I2c::I2c() : Engine(ENGINE_I2C) {
  addTask(TASK_CORE);
  addTask(TASK_DISCOVER);
  addTask(TASK_REPORT);
  addTask(TASK_COMMAND);

  gpio_config_t rst_pin_cfg;

  rst_pin_cfg.pin_bit_mask = RST_PIN_SEL;
  rst_pin_cfg.mode = GPIO_MODE_OUTPUT;
  rst_pin_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  rst_pin_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  rst_pin_cfg.intr_type = GPIO_INTR_DISABLE;

  gpio_config(&rst_pin_cfg);

  gpio_set_level(RST_PIN, 1); // bring all devices online
}

I2c_t *I2c::_instance_() {
  if (__singleton__ == nullptr) {
    __singleton__ = new I2c();
  }

  return __singleton__;
}

// STATIC!
bool I2c::engineEnabled() { return (__singleton__) ? true : false; }

//
// Tasks
//

void I2c::command(void *data) {
  _cmd_q = xQueueCreate(_max_queue_depth, sizeof(MsgPayload_t *));

  while (true) {
    BaseType_t queue_rc = pdFALSE;
    MsgPayload_t *payload = nullptr;
    elapsedMicros cmd_elapsed;

    _cmd_elapsed.reset();

    queue_rc = xQueueReceive(_cmd_q, &payload, portMAX_DELAY);
    // wrap in a unique_ptr so it is freed when out of scope
    unique_ptr<MsgPayload_t> payload_ptr(payload);

    if (queue_rc == pdFALSE) {
      Text::rlog("[i2c] [rc=%d] cmd queue receive failed", queue_rc);
      continue;
    }

    elapsedMicros parse_elapsed;
    // deserialize the msgpack data
    DynamicJsonDocument doc(_capacity);
    DeserializationError err = deserializeMsgPack(doc, payload->payload());

    // we're done with the original payload at this point
    // payload_ptr.reset();

    // parsing complete, freeze the elapsed timer
    parse_elapsed.freeze();

    // did the deserailization succeed?
    if (err) {
      Text::rlog("[%s] MSGPACK parse failure", err.c_str());
      continue;
    }

    string_t device = doc["device"] | "missing";
    I2cDevice_t *dev = findDevice(device);

    if (dev == nullptr) {
      Text::rlog("[i2c] could not locate device \"%s\"", device.c_str());
      continue;
    }

    bool ack = doc["ack"];
    RefID_t refid = doc["refid"];
    uint32_t cmd_mask = 0x00;
    uint32_t cmd_state = 0x00;

    // iterate through the array of new states
    JsonArray states = doc["states"].as<JsonArray>();
    for (JsonObject element : states) {
      // get a reference to the object from the array
      // const JsonObject &requested_state = element.as<JsonObject>();

      uint32_t bit = element["pio"];
      bool state = element["state"];

      // set the mask with each bit that should be adjusted
      cmd_mask |= (0x01 << bit);

      // set the tobe state with the values those bits should be
      // if the new_state is true (on) then set the bit,
      // otherwise leave it unset
      if (state) {
        cmd_state |= (0x01 << bit);
      }
    }

    commandExecute(dev, cmd_mask, cmd_state, ack, refid, cmd_elapsed);
  }
}

bool I2c::commandExecute(I2cDevice_t *dev, uint32_t cmd_mask,
                         uint32_t cmd_state, bool ack, const RefID_t &refid,
                         elapsedMicros &cmd_elapsed) {

  auto set_rc = false;

  if (dev->valid()) {

    needBus();
    takeBus();

    set_rc = dev->writeState(cmd_mask, cmd_state);

    if (set_rc) {
      commandAck(dev, ack, refid);
    }

    clearNeedBus();
    giveBus();
  }
  return set_rc;
}

void I2c::core(void *task_data) {

  bool driver_ready = false;

  while (!driver_ready) {
    driver_ready = installDriver();
    delay(1000); // prevent busy loop if i2c driver fails to install
  }

  pinReset();

  Net::waitForNormalOps();

  saveTaskLastWake(TASK_CORE);
  for (;;) {
    // signal to other tasks the dsEngine task is in it's run loop
    // this ensures all other set-up activities are complete before
    engineRunning();

    // do high-level engine actions here (e.g. general housekeeping)
    taskDelayUntil(TASK_CORE, _loop_frequency);
  }
}

void I2c::discover(void *data) {
  saveTaskLastWake(TASK_DISCOVER);

  while (waitForEngine()) {
    bool detect_rc = true;

    takeBus();
    detectMultiplexer();

    if (useMultiplexer()) {
      for (uint32_t bus = 0; (detect_rc && (bus < _mplex.maxBuses())); bus++) {
        detect_rc = detectDevicesOnBus(bus);
      }
    } else { // multiplexer not available, just search bus 0
      detect_rc = detectDevicesOnBus(0x00);
    }

    giveBus();

    if (numKnownDevices() > 0) {
      // signal to other tasks if there are devices available
      // after delaying a bit (to improve i2c bus stability)
      delay(500);
      devicesAvailable();
    }

    // we want to discover
    saveTaskLastWake(TASK_DISCOVER);
    taskDelayUntil(TASK_DISCOVER, _discover_frequency);
  }
}

void I2c::report(void *data) {
  saveTaskLastWake(TASK_REPORT);

  while (waitFor(devicesAvailableBit())) {
    if (numKnownDevices() == 0) {
      taskDelayUntil(TASK_REPORT, _report_frequency);
      continue;
    }

    Net::waitForNormalOps();

    for_each(beginDevices(), endDevices(), [this](I2cDevice_t *item) {
      I2cDevice_t *dev = item;

      if (dev->available()) {
        takeBus();

        if (selectBus(dev->bus()) && dev->read()) {
          publish(dev);
        }

        giveBus();

      } else {
        if (dev->missing()) {
          Text::rlog("device \"%s\" is missing", dev->id().c_str());
        }
      }
    });

    taskDelayUntil(TASK_REPORT, _report_frequency);
  }
}

// esp_err_t I2c::busRead(I2cDevice_t *dev, uint8_t *buff, uint32_t len,
//                        esp_err_t prev_esp_rc) {
//   i2c_cmd_handle_t cmd = nullptr;
//   esp_err_t esp_rc;
//
//   if (prev_esp_rc != ESP_OK) {
//     return prev_esp_rc;
//   }
//
//   int timeout = 0;
//   i2c_get_timeout(I2C_NUM_0, &timeout);
//
//   cmd = i2c_cmd_link_create(); // allocate i2c cmd queue
//   i2c_master_start(cmd);       // queue i2c START
//
//   i2c_master_write_byte(cmd, dev->readAddr(),
//                         true); // queue the READ for device and check for ACK
//
//   i2c_master_read(cmd, buff, len,
//                   I2C_MASTER_LAST_NACK); // queue the READ of number of bytes
//   i2c_master_stop(cmd);                  // queue i2c STOP
//
//   // execute queued i2c cmd
//   esp_rc = i2c_master_cmd_begin(I2C_NUM_0, cmd, _cmd_timeout);
//   i2c_cmd_link_delete(cmd);
//
//   if (esp_rc != ESP_OK) {
//     dev->readFailure();
//   }
//
//   return esp_rc;
// }

bool I2c::detectDevicesOnBus(int bus) {
  auto rc = false;
  SHT31_t sht31(bus);
  MCP23008_t mcp23008(bus);

  if (selectBus(bus) == false) {
    return false;
  }

  if (sht31.detect()) {
    rc = true;

    if (justSaw(sht31) == nullptr) {

      SHT31_t *new_dev = new SHT31(sht31, _dev_missing_secs);
      new_dev->setMissingSeconds(_report_frequency * 60 * 1.5);
      addDevice(new_dev);
    }
  }

  if (mcp23008.detect()) {
    rc = true;

    if (justSaw(mcp23008) == nullptr) {
      MCP23008_t *new_dev = new MCP23008(mcp23008, _dev_missing_secs);
      new_dev->setMissingSeconds(_report_frequency * 60 * 1.5);
      addDevice(new_dev);
    }
  }

  return rc;
}

bool I2c::detectMultiplexer() {
  _use_multiplexer = (Profile::i2cMultiplexer() && _mplex.detect());

  return _use_multiplexer;
}

bool I2c::installDriver() {
  esp_err_t config_rc = ESP_FAIL;
  esp_err_t install_rc = ESP_FAIL;
  auto rc = true;

  bzero(&_conf, sizeof(_conf));

  _conf.mode = I2C_MODE_MASTER;
  _conf.sda_io_num = (gpio_num_t)23;
  _conf.scl_io_num = (gpio_num_t)22;
  _conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
  _conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
  _conf.master.clk_speed = 100000;

  config_rc = i2c_param_config(I2C_NUM_0, &_conf);

  if (config_rc == ESP_OK) {
    install_rc = i2c_driver_install(I2C_NUM_0, _conf.mode, 0, 0, 0);
  }

  rc = ((config_rc == ESP_OK) && (install_rc == ESP_OK)) ? true : false;

  if (rc) {
    return rc;
  } else {
    Text::rlog("i2c failure config_rc=\"%s\" install_rc=\"%s\"",
               esp_err_to_name(config_rc), esp_err_to_name(install_rc));
  }

  return rc;
}

bool I2c::pinReset() {
  gpio_set_level(RST_PIN, 0); // pull the pin low to reset i2c devices
  delay(250);                 // give plenty of time for all devices to reset
  gpio_set_level(RST_PIN, 1); // bring all devices online
  delay(1000);                // give time for devices to initialize

  return true;
}

void I2c::printUnhandledDev(I2cDevice_t *dev) {
  Text::rlog("unhandled device \"%s\"", dev->debug().get());
}

bool I2c::selectBus(uint32_t bus) {
  if (useMultiplexer()) {
    return _mplex.selectBus(bus);
  }

  return true;
}

} // namespace ruth
