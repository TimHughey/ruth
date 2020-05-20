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
static const string_t engine_name = "I2c";

// command document capacity for expected metadata and up to eight pio states
const size_t _capacity =
    JSON_ARRAY_SIZE(8) + 8 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(7) + 227;

I2c::I2c() {
  EngineTask_t core("i2c", "core");
  EngineTask_t command("i2c", "command");
  EngineTask_t discover("i2c", "discover");
  EngineTask_t report("i2c", "report");

  addTask(engine_name, CORE, core);
  addTask(engine_name, COMMAND, command);
  addTask(engine_name, DISCOVER, discover);
  addTask(engine_name, REPORT, report);

  gpio_config_t rst_pin_cfg;

  rst_pin_cfg.pin_bit_mask = RST_PIN_SEL;
  rst_pin_cfg.mode = GPIO_MODE_OUTPUT;
  rst_pin_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  rst_pin_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  rst_pin_cfg.intr_type = GPIO_INTR_DISABLE;

  gpio_config(&rst_pin_cfg);
}

// STATIC!
bool I2c::engineEnabled() { return (__singleton__) ? true : false; }

//
// Tasks
//

void I2c::command(void *data) {
  logSubTaskStart(data);

  _cmd_q = xQueueCreate(_max_queue_depth, sizeof(MsgPayload_t *));

  while (true) {
    BaseType_t queue_rc = pdFALSE;
    MsgPayload_t *payload = nullptr;
    elapsedMicros cmd_elapsed;

    textReading_t *rlog = new textReading();
    textReading_ptr_t rlog_ptr(rlog);

    _cmd_elapsed.reset();

    queue_rc = xQueueReceive(_cmd_q, &payload, portMAX_DELAY);
    // wrap in a unique_ptr so it is freed when out of scope
    unique_ptr<MsgPayload_t> payload_ptr(payload);

    if (queue_rc == pdFALSE) {
      rlog->printf("[i2c] [rc=%d] cmd queue receive failed", queue_rc);
      rlog->publish();
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
      rlog->printf("[%s] MSGPACK parse failure", err.c_str());
      rlog->publish();
      continue;
    }

    string_t device = doc["device"] | "missing";
    i2cDev_t *dev = findDevice(device);

    if (dev == nullptr) {
      rlog->printf("[i2c] could not locate device \"%s\"", device.c_str());
      rlog->publish();
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

bool I2c::commandExecute(i2cDev_t *dev, uint32_t cmd_mask, uint32_t cmd_state,
                         bool ack, const RefID_t &refid,
                         elapsedMicros &cmd_elapsed) {
  textReading_t *rlog = new textReading();
  textReading_ptr_t rlog_ptr(rlog);

  // _latency_us.reset();

  if (dev->isValid()) {
    bool set_rc = false;

    needBus();
    takeBus();

    // the device write time is the total duration of all processing
    // of the write -- not just the duration on the bus
    dev->writeStart();

    set_rc = setMCP23008(dev, cmd_mask, cmd_state);

    dev->writeStop();

    if (set_rc) {
      commandAck(dev, ack, refid);
    }

    clearNeedBus();
    giveBus();

    return true;
  }
  return false;
}

void I2c::core(void *task_data) {
  bool driver_ready = false;

  while (!driver_ready) {
    driver_ready = installDriver();
    delay(1000); // prevent busy loop if i2c driver fails to install
  }

  pinReset();

  Net::waitForNormalOps();

  saveTaskLastWake(CORE);
  for (;;) {
    // signal to other tasks the dsEngine task is in it's run loop
    // this ensures all other set-up activities are complete before
    engineRunning();

    // do high-level engine actions here (e.g. general housekeeping)
    taskDelayUntil(CORE, _loop_frequency);
  }
}

void I2c::discover(void *data) {
  logSubTaskStart(data);
  saveTaskLastWake(DISCOVER);
  bool detect_rc = true;

  while (waitForEngine()) {

    takeBus();
    detectMultiplexer();

    if (useMultiplexer()) {
      for (uint32_t bus = 0; (detect_rc && (bus < maxBuses())); bus++) {
        detect_rc = detectDevicesOnBus(bus);
      }
    } else { // multiplexer not available, just search bus 0
      detect_rc = detectDevicesOnBus(0x00);
    }

    giveBus();

    // signal to other tasks if there are devices available
    // after delaying a bit (to improve i2c bus stability)
    delay(100);

    if (numKnownDevices() > 0) {
      devicesAvailable();
    }

    // we want to discover
    saveTaskLastWake(DISCOVER);
    taskDelayUntil(DISCOVER, _discover_frequency);
  }
}

void I2c::report(void *data) {

  logSubTaskStart(data);
  saveTaskLastWake(REPORT);

  while (waitFor(devicesAvailableBit())) {
    if (numKnownDevices() == 0) {
      taskDelayUntil(REPORT, _report_frequency);
      continue;
    }

    Net::waitForNormalOps();

    for_each(beginDevices(), endDevices(), [this](i2cDev_t *item) {
      i2cDev_t *dev = item;

      if (dev->available()) {
        takeBus();

        if (readDevice(dev)) {
          publish(dev);
        }

        giveBus();

      } else {
        if (dev->missing()) {
          textReading_t *rlog = new textReading();
          textReading_ptr_t rlog_ptr(rlog);

          rlog->printf("device \"%s\" is missing", dev->id().c_str());
          rlog->publish();
        }
      }
    });

    taskDelayUntil(REPORT, _report_frequency);
  }
}

esp_err_t I2c::busRead(i2cDev_t *dev, uint8_t *buff, uint32_t len,
                       esp_err_t prev_esp_rc) {
  i2c_cmd_handle_t cmd = nullptr;
  esp_err_t esp_rc;

  if (prev_esp_rc != ESP_OK) {
    return prev_esp_rc;
  }

  int timeout = 0;
  i2c_get_timeout(I2C_NUM_0, &timeout);

  cmd = i2c_cmd_link_create(); // allocate i2c cmd queue
  i2c_master_start(cmd);       // queue i2c START

  i2c_master_write_byte(cmd, dev->readAddr(),
                        true); // queue the READ for device and check for ACK

  i2c_master_read(cmd, buff, len,
                  I2C_MASTER_LAST_NACK); // queue the READ of number of bytes
  i2c_master_stop(cmd);                  // queue i2c STOP

  // execute queued i2c cmd
  esp_rc = i2c_master_cmd_begin(I2C_NUM_0, cmd, _cmd_timeout);
  i2c_cmd_link_delete(cmd);

  if (esp_rc != ESP_OK) {
    dev->readFailure();
  }

  return esp_rc;
}

esp_err_t I2c::busWrite(i2cDev_t *dev, uint8_t *bytes, uint32_t len,
                        esp_err_t prev_esp_rc) {
  i2c_cmd_handle_t cmd = nullptr;
  esp_err_t esp_rc;

  if (prev_esp_rc != ESP_OK) {
    return prev_esp_rc;
  }

  cmd = i2c_cmd_link_create(); // allocate i2c cmd queue
  i2c_master_start(cmd);       // queue i2c START

  i2c_master_write_byte(cmd, dev->writeAddr(),
                        true); // queue the device address (with WRITE)
                               // and check for ACK
  i2c_master_write(cmd, bytes, len,
                   true); // queue bytes to send (with ACK check)
  i2c_master_stop(cmd);   // queue i2c STOP

  // execute queued i2c cmd
  esp_rc = i2c_master_cmd_begin(I2C_NUM_0, cmd, _cmd_timeout);
  i2c_cmd_link_delete(cmd);

  if (esp_rc != ESP_OK) {
    dev->writeFailure();
  }

  return esp_rc;
}

bool I2c::crcSHT31(const uint8_t *data) {
  uint8_t crc = 0xFF;

  for (uint32_t j = 2; j; --j) {
    crc ^= *data++;

    for (uint32_t i = 8; i; --i) {
      crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
    }
  }

  // data was ++ in the above loop so it is already pointing at the crc
  return (crc == *data);
}

bool I2c::detectDevice(i2cDev_t *dev) {
  bool rc = false;
  // i2c_cmd_handle_t cmd = nullptr;
  esp_err_t esp_rc = ESP_FAIL;
  uint8_t sht31_cmd_data[] = {0x30, // soft-reset
                              0xa2};
  uint8_t detect_cmd[] = {dev->devAddr()};

  switch (dev->devAddr()) {

  case 0x70: // TCA9548B - TI i2c bus multiplexer
  case 0x44: // SHT-31 humidity sensor
    esp_rc = busWrite(dev, sht31_cmd_data, sizeof(sht31_cmd_data));
    break;
  case 0x20: // MCP23008 0x20 - 0x27
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x24:
  case 0x25:
  case 0x26:
  case 0x27:
  case 0x36: // STEMMA (seesaw based soil moisture sensor)
    esp_rc = busWrite(dev, detect_cmd, sizeof(detect_cmd));
    break;
  }

  if (esp_rc == ESP_OK) {
    rc = true;
  }

  return rc;
}

bool I2c::detectDevicesOnBus(int bus) {
  bool rc = false;

  DeviceAddress_t *addrs = search_addrs();

  for (uint8_t i = 0; addrs[i].isValid(); i++) {
    DeviceAddress_t &search_addr = addrs[i];
    i2cDev_t dev(search_addr, useMultiplexer(), bus);

    // abort detecting devices if the bus select fails
    if (selectBus(bus) == false)
      break;

    if (detectDevice(&dev)) {
      if (justSeenDevice(dev) == nullptr) {
        // device was not known, must add
        i2cDev_t *new_dev = new i2cDev(dev);
        addDevice(new_dev);
      }

      devicesAvailable();
      rc = true;
    }
  }

  return rc;
}

bool I2c::detectMultiplexer(const int max_attempts) {
  // _use_multiplexer initially set based on profile
  // however is updated based on actual multiplexer detection
  _use_multiplexer = Profile::i2cUseMultiplexer();

  if (_use_multiplexer && detectDevice(&_multiplexer_dev)) {
    _use_multiplexer = true;
  } else {
    _use_multiplexer = false;
  }

  return _use_multiplexer;
}

// 2020-05-20: the stabblity of the I2c SDK has improved and this code
// is unncessary

// bool I2c::hardReset() {
//   esp_err_t rc;
//
//   delay(1000);
//
//   rc = i2c_driver_delete(I2C_NUM_0);
//
//   periph_module_disable(PERIPH_I2C0_MODULE);
//   periph_module_enable(PERIPH_I2C0_MODULE);
//
//   return installDriver();
// }
//
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
    textReading_t *rlog = new textReading();
    textReading_ptr_t rlog_ptr(rlog);

    rlog->printf("i2c failure config_rc=\"%s\" install_rc=\"%s\"",
                 esp_err_to_name(config_rc), esp_err_to_name(install_rc));
    rlog->publish();
  }

  // 2020-05-20: the delay below is likely unncessary because of improved
  //             i2c SDK stability
  // delay(1000);

  return rc;
}

I2c_t *I2c::_instance_() {
  if (__singleton__ == nullptr) {
    __singleton__ = new I2c();
  }

  return __singleton__;
}

uint32_t I2c::maxBuses() { return _max_buses; }
bool I2c::pinReset() {

  gpio_set_level(RST_PIN, 0); // pull the pin low to reset i2c devices
  delay(250);                 // give plenty of time for all devices to reset
  gpio_set_level(RST_PIN, 1); // bring all devices online

  return true;
}

void I2c::printUnhandledDev(i2cDev_t *dev) {
  textReading_t *rlog = new textReading();
  textReading_ptr_t rlog_ptr(rlog);

  rlog->printf("unhandled device \"%s\"", dev->debug().get());
  rlog->publish();
}

bool I2c::useMultiplexer() { return _use_multiplexer; }

bool I2c::readDevice(i2cDev_t *dev) {
  auto rc = false;

  if (selectBus(dev->bus())) {
    switch (dev->devAddr()) {

    case 0x44:
      rc = readSHT31(dev);
      break;

    case 0x20: // MCP23008 can be user configured to 0x20 + three bits
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
      rc = readMCP23008(dev);
      break;

    case 0x36: // Seesaw Soil Probe
      rc = readSeesawSoil(dev);
      break;

    default:
      printUnhandledDev(dev);
      rc = true;
      break;
    }
  }

  return rc;
}

bool I2c::readMCP23008(i2cDev_t *dev) {
  auto rc = false;
  auto positions = 0b00000000;
  esp_err_t esp_rc;

  RawData_t request{0x00}; // IODIR Register (address 0x00)

  // register       register      register          register
  // 0x00 - IODIR   0x01 - IPOL   0x02 - GPINTEN    0x03 - DEFVAL
  // 0x04 - INTCON  0x05 - IOCON  0x06 - GPPU       0x07 - INTF
  // 0x08 - INTCAP  0x09 - GPIO   0x0a - OLAT

  // at POR the MCP2x008 operates in sequential mode where continued reads
  // automatically increment the address (register).  we read all registers
  // (12 bytes) in one shot.
  RawData_t all_registers;
  all_registers.resize(12); // 12 bytes (0x00-0x0a)

  esp_rc = requestData(dev, request.data(), request.size(),
                       all_registers.data(), all_registers.capacity());

  if (esp_rc == ESP_OK) {
    // GPIO register is little endian so no conversion is required
    positions = all_registers[0x0a]; // OLAT register (address 0x0a)

    dev->storeRawData(all_registers);

    dev->justSeen();

    positionsReading_t *reading =
        new positionsReading(dev->id(), time(nullptr), positions, (uint8_t)8);

    reading->setLogReading();
    dev->setReading(reading);
    rc = true;
  } else {
    textReading_t *rlog = new textReading();
    textReading_ptr_t rlog_ptr(rlog);

    rlog->printf("[%s] device \"%s\" read issue", esp_err_to_name(esp_rc),
                 dev->id().c_str());
    rlog->publish();
  }

  return rc;
}

bool I2c::readSeesawSoil(i2cDev_t *dev) {
  auto rc = false;
  esp_err_t esp_rc;
  float tempC = 0.0;
  int soil_moisture;
  // int sw_version;

  // seesaw data queries are two bytes that describe:
  //   1. module
  //   2. register
  // NOTE: array is ONLY for the transmit since the response varies
  //       by module and register
  uint8_t data_request[] = {0x00,  // MSB: module
                            0x00}; // LSB: register

  // seesaw responses to data queries vary in length.  this buffer will be
  // reused for all queries so it must be the max of all response lengths
  //   1. capacitance - 16bit integer (two bytes)
  //   2. temperature - 32bit float (four bytes)
  uint8_t buff[] = {
      0x00, 0x00, // capactive: (int)16bit, temperature: (float)32 bits
      0x00, 0x00  // capactive: not used, temperature: (float)32 bits
  };

  // address i2c device
  // write request to read module and register
  //  temperture: SEESAW_STATUS_BASE, SEEWSAW_STATUS_TEMP  (4 bytes)
  //     consider other status: SEESAW_STATUS_HW_ID, *VERSION, *OPTIONS
  //  capacitance:  SEESAW_TOUCH_BASE, SEESAW_TOUCH_CHANNEL_OFFSET (2 bytes)
  // delay (maybe?)
  // write request to read bytes of the register

  dev->readStart();

  // first, request and receive the onboard temperature
  data_request[0] = 0x00; // SEESAW_STATUS_BASE
  data_request[1] = 0x04; // SEESAW_STATUS_TEMP
  esp_rc = busWrite(dev, data_request, 2);
  delay(20);
  esp_rc = busRead(dev, buff, 4, esp_rc);

  // conversion copied from AdaFruit Seesaw library
  tempC = (1.0 / (1UL << 16)) *
          (float)(((uint32_t)buff[0] << 24) | ((uint32_t)buff[1] << 16) |
                  ((uint32_t)buff[2] << 8) | (uint32_t)buff[3]);

  // second, request and receive the touch capacitance (soil moisture)
  data_request[0] = 0x0f; // SEESAW_TOUCH_BASE
  data_request[1] = 0x10; // SEESAW_TOUCH_CHANNEL_OFFSET

  esp_rc = busWrite(dev, data_request, 2);
  delay(20);
  esp_rc = busRead(dev, buff, 2, esp_rc);

  soil_moisture = ((uint16_t)buff[0] << 8) | buff[1];

  // third, request and receive the board version
  // data_request[0] = 0x00; // SEESAW_STATUS_BASE
  // data_request[1] = 0x02; // SEESAW_STATUS_VERSION
  //
  // esp_rc = bus_write(dev, data_request, 2);
  // esp_rc = bus_read(dev, buff, 4, esp_rc);
  //
  // sw_version = ((uint32_t)buff[0] << 24) | ((uint32_t)buff[1] << 16) |
  //              ((uint32_t)buff[2] << 8) | (uint32_t)buff[3];

  dev->readStop();

  if (esp_rc == ESP_OK) {
    dev->justSeen();

    soilReading_t *reading = new soilReading(dev->id(), tempC, soil_moisture);

    dev->setReading(reading);
    rc = true;
  } else {
    textReading_t *rlog = new textReading();
    textReading_ptr_t rlog_ptr(rlog);

    rlog->printf("[%s] device \"%s\" read issue", esp_err_to_name(esp_rc),
                 dev->id().c_str());
    rlog->publish();
  }

  return rc;
}

bool I2c::readSHT31(i2cDev_t *dev) {
  auto rc = false;
  esp_err_t esp_rc;

  uint8_t request[] = {
      0x2c, // single-shot measurement, with clock stretching
      0x06  // high-repeatability measurement (max duration 15ms)
  };
  uint8_t buff[] = {
      0x00, 0x00, // tempC high byte, low byte
      0x00,       // crc8 of temp
      0x00, 0x00, // relh high byte, low byte
      0x00        // crc8 of relh
  };

  esp_rc = requestData(dev, request, sizeof(request), buff, sizeof(buff));

  if (esp_rc == ESP_OK) {
    dev->justSeen();

    if (crcSHT31(buff) && crcSHT31(&(buff[3]))) {
      // conversion from SHT31 datasheet
      uint16_t stc = (buff[0] << 8) | buff[1];
      uint16_t srh = (buff[3] << 8) | buff[4];

      float tc = (float)((stc * 175) / 0xffff) - 45;
      float rh = (float)((srh * 100) / 0xffff);

      humidityReading_t *reading = new humidityReading(dev->id(), tc, rh);

      dev->setReading(reading);

      rc = true;
    } else { // crc did not match

      dev->crcMismatch();

      auto *rlog = new textReading();
      textReading_ptr_t rlog_ptr(rlog);

      rlog->printf("device \"%s\" crc check failed", esp_err_to_name(esp_rc),
                   dev->id().c_str());
      rlog->publish();
    }
  } else { // esp_rc != ESP_OK
    textReading_t *rlog = new textReading();
    textReading_ptr_t rlog_ptr(rlog);

    rlog->printf("[%s] device \"%s\" read issue", esp_err_to_name(esp_rc),
                 dev->id().c_str());
    rlog->publish();
  }

  return rc;
}

esp_err_t I2c::requestData(i2cDev_t *dev, uint8_t *send, uint8_t send_len,
                           uint8_t *recv, uint8_t recv_len,
                           esp_err_t prev_esp_rc, int timeout) {
  i2c_cmd_handle_t cmd = nullptr;
  esp_err_t esp_rc;

  dev->readStart();

  if (prev_esp_rc != ESP_OK) {
    dev->readFailure();
    dev->readStop();
    return prev_esp_rc;
  }

  int _save_timeout = 0;
  if (timeout > 0) {
    i2c_get_timeout(I2C_NUM_0, &_save_timeout);
    i2c_set_timeout(I2C_NUM_0, timeout);
  }

  cmd = i2c_cmd_link_create(); // allocate i2c cmd queue
  i2c_master_start(cmd);       // queue i2c START

  i2c_master_write_byte(cmd, dev->writeAddr(),
                        true); // queue the WRITE for device and check for ACK

  i2c_master_write(cmd, send, send_len,
                   I2C_MASTER_ACK); // queue the device command bytes

  // clock stretching is leveraged in the event the device requires time
  // to execute the command (e.g. temperature conversion)
  // use timeout to adjust time to wait for clock, if needed

  if ((recv != nullptr) && (recv_len > 0)) {
    // start a new command sequence without sending a stop
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, dev->readAddr(),
                          true); // queue the READ for device and check for ACK

    i2c_master_read(cmd, recv, recv_len,
                    I2C_MASTER_LAST_NACK); // queue the READ of number of bytes
    i2c_master_stop(cmd);                  // queue i2c STOP
  }

  // execute queued i2c cmd
  esp_rc = i2c_master_cmd_begin(I2C_NUM_0, cmd, _cmd_timeout);
  i2c_cmd_link_delete(cmd);

  if (esp_rc != ESP_OK) {
    dev->readFailure();
  }

  // if the timeout was changed restore it
  if (_save_timeout > 0) {
    i2c_set_timeout(I2C_NUM_0, _save_timeout);
  }

  dev->readStop();

  return esp_rc;
}

bool I2c::selectBus(uint32_t bus) {
  bool rc = true; // default return is success, failures detected inline
  i2cDev_t multiplexer = i2cDev(_multiplexer_dev);
  esp_err_t esp_rc = ESP_FAIL;

  _bus_selects++;

  if (bus >= _max_buses) {
    return false;
  }

  if (useMultiplexer() && (bus < _max_buses)) {
    // the bus is selected by sending a single byte to the multiplexer
    // device with the bit for the bus select
    uint8_t bus_cmd[1] = {(uint8_t)(0x01 << bus)};

    esp_rc = busWrite(&multiplexer, bus_cmd, 1);

    if (esp_rc != ESP_OK) {
      _bus_select_errors++;
      rc = false;

      if (_bus_select_errors > 50) {
        const char *msg = "BUS SELECT ERRORS EXCEEDED";
        NVS::commitMsg("I2c", msg);
        Restart::restart(msg, __PRETTY_FUNCTION__, 0);
      }
    }
  }

  return rc;
}

bool I2c::setMCP23008(i2cDev_t *dev, uint32_t cmd_mask, uint32_t cmd_state) {
  bool rc = false;
  auto esp_rc = ESP_OK;

  textReading *rlog = new textReading_t;
  textReading_ptr_t rlog_ptr(rlog);
  RawData_t tx_data;

  tx_data.reserve(12);

  // read the device to ensure we have the current state
  // important because setting the new state relies, in part, on the existing
  // state for the pios not changing
  if (readDevice(dev) == false) {
    rlog->reuse();
    rlog->printf("device \"%s\" read before set failed", dev->debug().get());
    rlog->publish();

    return rc;
  }

  positionsReading_t *reading = (positionsReading_t *)dev->reading();

  // if register 0x00 (IODIR) is not 0x00 (IODIR isn't output) then
  // set it to output
  if (dev->rawData().at(0) > 0x00) {
    tx_data.insert(tx_data.end(), {0x00, 0x00});
    esp_rc =
        requestData(dev, tx_data.data(), tx_data.size(), nullptr, 0, esp_rc);
  }

  auto asis_state = reading->state();
  auto new_state = 0x00;

  // XOR the new state against the as_is state using the mask
  // it is critical that we use the recently read state to avoid
  // overwriting the device state that MCP is not aware of
  new_state = asis_state ^ ((asis_state ^ cmd_state) & cmd_mask);

  // to set the GPIO we will write to two registers:
  // a. IODIR (0x00) - setting all GPIOs to output (0b00000000)
  // b. OLAT (0x0a)  - the new state
  tx_data.clear();
  tx_data.insert(tx_data.end(), {0x0a, (uint8_t)(new_state & 0xff)});

  esp_rc = requestData(dev, tx_data.data(), tx_data.size(), nullptr, 0, esp_rc);

  if (esp_rc != ESP_OK) {
    rlog->reuse();
    rlog->printf("[%s] device \"%s\" set failed", esp_err_to_name(esp_rc),
                 dev->debug().get());
    rlog->publish();

    return rc;
  }

  rc = true;
  rlog->publish();

  return rc;
}

} // namespace ruth
