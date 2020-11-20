/*
          DallasSemi - Ruth Dallas Semiconductor
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

#include <cstdint>
#include <cstdlib>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "engines/ds.hpp"

namespace ruth {

DallasSemi_t *__singleton__ = nullptr;

// command document capacity for expected metadata and up to eight pio states
const size_t _capacity =
    JSON_ARRAY_SIZE(8) + 8 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(7) + 227;

DallasSemi::DallasSemi() : Engine(ENGINE_DALSEMI) {
  addTask(TASK_CORE);
  addTask(TASK_DISCOVER);
  addTask(TASK_CONVERT);
  addTask(TASK_REPORT);
  addTask(TASK_COMMAND);
}

// STATIC!
bool DallasSemi::engineEnabled() { return (__singleton__) ? true : false; }

bool DallasSemi::checkDevicesPowered() {
  bool rc = false;
  owb_status owb_s;
  uint8_t read_pwr_cmd[] = {0xcc, 0xb4};
  uint8_t pwr = 0x00;

  // reset the bus before and after the power check since this
  // method may be called in conjuction of other bus operations
  resetBus();

  owb_s = owb_write_bytes(_ds, read_pwr_cmd, sizeof(read_pwr_cmd));
  owb_s = owb_read_byte(_ds, &pwr);

  if ((owb_s == OWB_STATUS_OK) && pwr) {
    rc = true;
  }

  resetBus();

  return rc;
}

void DallasSemi::command(void *data) {
  _cmd_q = xQueueCreate(_max_queue_depth, sizeof(MsgPayload_t *));

  // no setup required before jumping into task loop

  while (true) {
    BaseType_t queue_rc = pdFALSE;
    MsgPayload_t *payload = nullptr;
    elapsedMicros cmd_elapsed;

    _cmd_elapsed.reset();

    clearNeedBus();
    queue_rc = xQueueReceive(_cmd_q, &payload, portMAX_DELAY);
    // wrap in a unique_ptr so it is freed when out of scope
    unique_ptr<MsgPayload_t> payload_ptr(payload);

    if (queue_rc == pdFALSE) {
      Text::rlog("[DalSemi] [rc=%d] queue receive failed", queue_rc);

      continue;
    }

    elapsedMicros parse_elapsed;
    // deserialize the msgpack data
    StaticJsonDocument<_capacity> doc;
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
    const char *device = doc["device"] | "missing";
    DsDevice_t *dev = findDevice(device);

    if (dev == nullptr) {
      Text::rlog("[DalSemi] could not find device \"%s\"", device);

      continue;
    }

    bool ack = doc["ack"];
    const RefID_t refid = doc["refid"].as<const char *>();
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

bool DallasSemi::commandExecute(DsDevice_t *dev, uint32_t cmd_mask,
                                uint32_t cmd_state, bool ack,
                                const RefID_t &refid,
                                elapsedMicros &cmd_elapsed) {
  auto set_rc = false;
  if (dev && dev->valid()) {

    needBus();
    takeBus();

    // the device write time is the total duration of all processing
    // of the write -- not just the duration on the bus
    dev->writeStart();

    if (dev->isDS2406()) {
      set_rc = setDS2406(dev, cmd_mask, cmd_state);
    } else if (dev->isDS2408()) {
      set_rc = setDS2408(dev, cmd_mask, cmd_state);
    } else if (dev->isDS2413()) {
      set_rc = setDS2413(dev, cmd_mask, cmd_state);
    }

    dev->writeStop();

    if (set_rc) {
      commandAck(dev, ack, refid);
    }

    giveBus();
  }

  return set_rc;
}

// SubTasks receive their task config via the void *data
void DallasSemi::convert(void *data) {
  uint8_t temp_convert_cmd[] = {0xcc, 0x44};

  // ensure the temp available bit is cleared at task startup
  tempUnavailable();

  for (;;) {
    bool present = false;
    uint8_t data = 0x00;
    owb_status owb_s;

    // wait here for devices available bit
    waitFor(devicesOrTempSensorsBit());

    // now that the wait has been satisified record the last wake time
    // this is important to avoid performing the convert too frequently
    saveTaskLastWake(TASK_CONVERT);

    // use event bits to signal when there are temperatures available
    // start by clearing the bit to signal there isn't a temperature available
    tempUnavailable();

    if (!devicesPowered() && !tempDevicesPresent()) {
      taskDelayUntil(TASK_CONVERT, _convert_frequency);
      continue;
    }

    takeBus();

    if (resetBus(&present) && (present == false)) {
      giveBus();
      taskDelayUntil(TASK_CONVERT, _convert_frequency);
      continue;
    }

    resetBus();
    owb_s = owb_write_bytes(_ds, temp_convert_cmd, sizeof(temp_convert_cmd));
    owb_s = owb_read_byte(_ds, &data);

    // before dropping into waiting for the temperature conversion to
    // complete let's double check there weren't any errors after initiating
    // the convert.  additionally, we should see zeroes on the bus since
    // devices will hold the bus low during the convert
    if ((owb_s != OWB_STATUS_OK) || (data != 0x00)) {
      giveBus();
      taskDelayUntil(TASK_CONVERT, _convert_frequency);
      continue;
    }

    bool in_progress = true;
    bool temp_available = false;
    uint64_t _wait_start = esp_timer_get_time();
    while ((owb_s == OWB_STATUS_OK) && in_progress) {
      owb_s = owb_read_byte(_ds, &data);

      if (owb_s != OWB_STATUS_OK) {

        Text::rlog("[DalSemi] temp convert failed (0x%02x)", owb_s);

        break;
      }

      // if the bus isn't low then the convert is finished
      if (data > 0x00) {
        // NOTE: use a flag here so we can exit the while loop before
        // setting the event group bit since tasks waiting for the bit
        // will immediately wake up.  this allows for clean tracking of
        // the temp convert elapsed time.
        temp_available = true;
        in_progress = false;
      }

      if (in_progress) {
        BaseType_t notified = pdFALSE;

        // wait for time to pass or to be notified that another
        // task needs the bus
        // if the bit was set then clear it
        EventBits_t bits = waitFor(needBusBit(), _temp_convert_wait, true);
        notified = (bits & needBusBit());

        // another task needs the bus so break out of the loop
        if (notified) {
          resetBus();          // abort the temperature convert
          in_progress = false; // signal to break from loop
        }

        if ((esp_timer_get_time() - _wait_start) >= _max_temp_convert_us) {

          Text::rlog("[DalSemi] temp convert timed out)");

          resetBus();
          in_progress = false; // signal to break from loop
        }
      }
    }

    giveBus();

    // signal to other tasks if temperatures are available
    if (temp_available) {
      tempAvailable();
    }

    taskDelayUntil(TASK_CONVERT, _convert_frequency);
  }
}

void DallasSemi::discover(void *data) {
  saveTaskLastWake(TASK_DISCOVER);

  while (waitForEngine()) {
    owb_status owb_s;

    bool found = false;
    auto device_found = false;
    auto have_temperature_devs = false;
    OneWireBus_SearchState search_state;

    bzero(&search_state, sizeof(OneWireBus_SearchState));

    // take the bus before beginning time tracking to avoid
    // artificially inflating discover elapsed time
    takeBus();

    bool present = false;
    if (resetBus(&present) && (present == false)) {
      giveBus();
      taskDelayUntil(TASK_DISCOVER, _discover_frequency);
      continue;
    }

    owb_s = owb_search_first(_ds, &search_state, &found);

    if (owb_s != OWB_STATUS_OK) {
      giveBus();
      taskDelayUntil(TASK_DISCOVER, _discover_frequency);
      continue;
    }

    bool hold_bus = true;
    while ((owb_s == OWB_STATUS_OK) && found && hold_bus) {
      device_found = true;

      DeviceAddress_t found_addr(search_state.rom_code.bytes, 8);
      DsDevice_t dev(found_addr, true);

      if (justSeenDevice(dev) == nullptr) {
        DsDevice_t *new_dev = new DsDevice(dev);
        new_dev->setMissingSeconds(_report_frequency * 60 * 1.5);
        addDevice(new_dev);
      }

      if (dev.hasTemperature()) {
        have_temperature_devs = true;
      }

      // another task needs the bus so break out of the loop
      if (isBusNeeded()) {
        resetBus();       // abort the search
        hold_bus = false; // signal to break from loop
      } else {
        // keeping searching
        owb_s = owb_search_next(_ds, &search_state, &found);

        if (owb_s != OWB_STATUS_OK) {

          Text::rlog("DalSemi search next failed owb_s=\"%d\"", owb_s);
        }
      }
    }

    // TODO: create specific logic to detect pwr status of each family code
    // ds->reset();
    // ds->select(addr);
    // ds->write(0xB4); // Read Power Supply
    // uint8_t pwr = ds->read_bit();

    _devices_powered = checkDevicesPowered();

    giveBus();

    // must set before setting devices_available
    _temp_devices_present = have_temperature_devs;
    temperatureSensors(have_temperature_devs);

    // signal to other tasks if there are devices available
    devicesAvailable(device_found);

    // to avoid including the execution time of the discover phase
    saveTaskLastWake(TASK_DISCOVER);
    taskDelayUntil(TASK_DISCOVER, _discover_frequency);
  }
}

DallasSemi_t *DallasSemi::_instance_() {
  if (__singleton__ == nullptr) {
    __singleton__ = new DallasSemi();
  }

  return __singleton__;
}

void DallasSemi::report(void *data) {
  Net::waitForNormalOps();

  // let's wait here for the signal devices are available
  // important to ensure we don't start reporting before
  // the rest of the system is fully available (e.g. wifi, mqtt)
  while (waitFor(devicesAvailableBit())) {
    Net::waitForNormalOps();

    // there are two cases of when report should run:
    //  a. wait for a temperature if there are temperature devices
    //  b. wait a preset duration

    // case a:  wait for temperature to be available
    if (_temp_devices_present) {
      // let's wait here for the temperature available bit
      // once we see it then clear it to ensure we don't run again until
      // it's available again
      waitFor(temperatureAvailableBit(), _report_frequency, true);
    }

    // last wake is after the event group has been satisified
    saveTaskLastWake(TASK_REPORT);

    for_each(beginDevices(), endDevices(), [this](DsDevice_t *item) {
      DsDevice_t *dev = item;

      if (dev->available()) {
        takeBus();

        if (readDevice(dev)) {
          publish(dev);
        }

        giveBus();
      }
    });

    // case b:  wait a preset duration (no temp devices)
    if (_temp_devices_present == false) {
      taskDelayUntil(TASK_REPORT, _report_frequency);
    }
  }
}

bool DallasSemi::readDevice(DsDevice_t *dev) {
  Sensor_t *reading = nullptr;
  Positions_t *positions = nullptr;
  auto rc = false;

  if (dev->notValid()) {
    return false;
  }

  // before attempting to read any device reset the bus.
  // if the reset fails then something has gone wrong with the bus
  // perform this check here so the specialized read methods can assume
  // the bus is operational and eliminate redundant code
  if (resetBus() == false) {
    return rc;
  }

  switch (dev->family()) {
  case 0x10: // DS1820 (temperature sensors)
  case 0x22:
  case 0x28:
    dev->readStart();
    rc = readDS1820(dev, &reading);
    dev->readStop();
    if (rc)
      dev->setReading(reading);
    break;

  case 0x29: // DS2408 (8-channel switch)
    rc = readDS2408(dev, &positions);
    if (rc)
      dev->setReading(positions);
    break;

  case 0x12: // DS2406 (2-channel switch with 1k EPROM)
    rc = readDS2406(dev, &positions);
    if (rc)
      dev->setReading(positions);
    break;

  case 0x3a: // DS2413 (Dual-Channel Addressable Switch)
    rc = readDS2413(dev, &positions);
    if (rc) {
      dev->setReading(positions);
    }
    break;

  case 0x26: // DS2438 (Smart Battery Monitor)
    rc = false;
    break;

  default:
    rc = false;
  }

  return rc;
}

// specific device scratchpad methods
bool DallasSemi::readDS1820(DsDevice_t *dev, Sensor_t **reading) {
  owb_status owb_s;
  uint8_t data[9] = {0x00};
  bool type_s = false;
  bool rc = false;

  switch (dev->family()) {
  case 0x10:
    type_s = true;
    break;
  default:
    type_s = false;
  }

  dev->readStart();
  uint8_t cmd[] = {0x55, // match rom_code
                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // rom
                   0xbe}; // read scratchpad

  dev->copyAddrToCmd(cmd);

  owb_s = owb_write_bytes(_ds, cmd, sizeof(cmd));
  owb_s = owb_read_bytes(_ds, data, sizeof(data));
  resetBus();

  if (owb_s != OWB_STATUS_OK) {

    Text::rlog("device \"%s\" read failure owb_s=\"%d\"", dev->id(), owb_s);

    return rc;
  }

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    uint32_t cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00)
      raw = raw & ~7; // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20)
      raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40)
      raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }

  // calculate the crc of the received scratchpad
  uint16_t crc8 = owb_crc8_bytes(0x00, data, sizeof(data));

  if (crc8 != 0x00) {

    Text::rlog("device \"%s\" failed crc8=\"0x%02x\"", dev->id(), crc8);

    return rc;
  }

  float celsius = (float)raw / 16.0;

  rc = true;
  *reading = new Sensor(dev->id(), celsius);

  return rc;
}

bool DallasSemi::readDS2406(DsDevice_t *dev, Positions_t **reading) {
  owb_status owb_s;
  bool rc = false;

  uint8_t cmd[] = {0x55, // match rom_code
                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // rom
                   0xaa,        // read status cmd
                   0x00, 0x00}; // address (start at beginning)

  uint8_t buff[] = {0x00, 0x00, 0x00, 0x00, 0x00, // byte 0-4: EPROM bitmaps
                    0x00, // byte 5: EPROM Factory Test Byte
                    0x00, // byte 6: Don't care (always reads 0x00)
                    0x00, // byte 7: SRAM (channel flip-flops, power, etc.)
                    0x00, 0x00}; // byte 8-9: CRC16

  dev->readStart();
  dev->copyAddrToCmd(cmd);

  owb_s = owb_write_bytes(_ds, cmd, sizeof(cmd));

  if (owb_s != OWB_STATUS_OK) {

    Text::rlog("device \"%s\" cmd failure owb_s=\"%d\"", dev->id(), owb_s);

    return rc;
  }

  // fill buffer with bytes from DS2406, skipping the first byte
  // since the first byte is included in the CRC16
  owb_s = owb_read_bytes(_ds, buff, sizeof(buff));
  dev->readStop();

  if (owb_s != OWB_STATUS_OK) {

    Text::rlog("device \"%s\" read failure owb_s=\"%d\"", dev->id(), owb_s);

    return rc;
  }

  resetBus();

  uint32_t raw = buff[sizeof(buff) - 3];

  uint32_t positions = 0x00;  // translate raw status to 0b000000xx
  if ((raw & 0x20) == 0x00) { // to represent PIO.A as bit 0
    positions = 0x01;         // and PIO.B as bit 1
  }                           // reminder to invert the bits since the device
                              // represents on/off opposite of true/false
  if ((raw & 0x40) == 0x00) {
    positions = (positions | 0x02);
  }

  rc = true;
  *reading = new Positions(dev->id(), positions, (uint8_t)2);

  return rc;
}

bool DallasSemi::readDS2408(DsDevice_t *dev, Positions_t **reading) {
  owb_status owb_s;
  bool rc = false;

  // byte data[12]
  uint8_t dev_cmd[] = {
      0x55,                                           // byte 0: match ROM
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // byte 1-8: rom
      0xf5,                   // byte 9: channel access read cmd
      0x00, 0x00, 0x00, 0x00, // bytes 10-41: channel state data
      0x00, 0x00, 0x00, 0x00, // channel state data
      0x00, 0x00, 0x00, 0x00, // channel state data
      0x00, 0x00, 0x00, 0x00, // channel state data
      0x00, 0x00, 0x00, 0x00, // channel state data
      0x00, 0x00, 0x00, 0x00, // channel state data
      0x00, 0x00, 0x00, 0x00, // channel state data
      0x00, 0x00, 0x00, 0x00, // channel state data
      0x00, 0x00};            // bytes 42-43: CRC16

  dev->readStart();
  dev->copyAddrToCmd(dev_cmd);

  // send bytes through the Channel State Data device command
  owb_s = owb_write_bytes(_ds, dev_cmd, 10);

  if (owb_s != OWB_STATUS_OK) {
    dev->readStop();
    return rc;
  }

  // read 32 bytes of channel state data + 16 bits of CRC
  // into the dev_cmd
  owb_s = owb_read_bytes(_ds, (dev_cmd + 10), 34);
  dev->readStop();

  if (owb_s != OWB_STATUS_OK) {

    Text::rlog("device \"%s\" failed to read cmd result owb_s=\"%d\"",
               dev->id(), owb_s);

    return rc;
  }

  resetBus();

  // compute the crc16 over the Channel Access command through channel state
  // data (excluding the crc16 bytes)
  uint16_t crc16 =
      check_crc16((dev_cmd + 9), 33, &(dev_cmd[sizeof(dev_cmd) - 2]));

  if (!crc16) {

    Text::rlog("device \"%s\" failed crc16=\"0x%02x\"", dev->id(), crc16);

    return rc;
  }

  // negate positions since device sees on/off opposite of true/false
  // uint32_t positions = (~buff[sizeof(buff) - 3]) & 0xFF; // constrain to
  // 8bits
  uint32_t positions =
      ~(dev_cmd[sizeof(dev_cmd) - 3]) & 0xFF; // constrain to 8bits

  if (reading != nullptr) {
    *reading = new Positions(dev->id(), positions, (uint32_t)8);
  }
  rc = true;

  return rc;
}

bool DallasSemi::readDS2413(DsDevice_t *dev, Positions_t **reading) {
  owb_status owb_s;
  bool rc = false;

  uint8_t cmd[] = {0x55, // match rom_code
                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // rom
                   0xf5}; // pio access read

  uint8_t buff[] = {0x00, 0x00}; // byte 0-1: pio status bit assignment (x2)

  dev->readStart();
  dev->copyAddrToCmd(cmd);

  owb_s = owb_write_bytes(_ds, cmd, sizeof(cmd));

  if (owb_s != OWB_STATUS_OK) {
    dev->readStop();

    Text::rlog("device \"%s\" cmd failure owb_s=\"%d\"", dev->id(), owb_s);

    return rc;
  }

  // fill buffer with bytes from DS2406, skipping the first byte
  // since the first byte is included in the CRC16
  owb_s = owb_read_bytes(_ds, buff, sizeof(buff));
  dev->readStop();

  if (owb_s != OWB_STATUS_OK) {
    return rc;
  }

  resetBus();

  // both bytes should be the same
  if (buff[0] != buff[1]) {

    Text::rlog("device \"%s\" state byte0=\"0x%02x\" and byte1=\"0x%02x\" "
               "should match",
               dev->id(), buff[0], buff[1]);

    return rc;
  }

  uint32_t raw = buff[0];

  // NOTE: pio states are inverted at the device
  // PIO Status Bits:
  //    b0:    PIOA State
  //    b1:    PIOA Latch
  //    b2:    PIOB State
  //    b3:    PIOB Latch
  //    b4-b7: complement of b3 to b0
  uint32_t positions = 0x00;  // translate raw status to 0b000000xx
  if ((raw & 0x01) == 0x00) { // represent PIO.A as bit 0
    positions = 0x01;
  }

  if ((raw & 0x04) == 0x00) { // represent PIO.B as bit 1
    positions = (positions | 0x02);
  }

  rc = true;
  *reading = new Positions(dev->id(), positions, (uint8_t)2);

  return rc;
}

bool DallasSemi::resetBus(bool *present) {
  auto __present = false;
  owb_status owb_s;

  owb_s = owb_reset(_ds, &__present);

  if (present != nullptr) {
    *present = __present;
  }

  if (owb_s == OWB_STATUS_OK) {
    return true;
  }

  return false;
}

void DallasSemi::core(void *data) {
  owb_rmt_driver_info *rmt_driver = new owb_rmt_driver_info;
  _ds = owb_rmt_initialize(rmt_driver, _pin, RMT_CHANNEL_0, RMT_CHANNEL_1);

  owb_use_crc(_ds, true);

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

bool DallasSemi::setDS2406(DsDevice_t *dev, uint32_t cmd_mask,
                           uint32_t cmd_state) {
  owb_status owb_s;
  bool rc = false;

  Positions_t *reading = (Positions_t *)dev->reading();

  uint32_t asis_state = reading->state();

  bool pio_a = (cmd_mask & 0x01) ? (cmd_state & 0x01) : (asis_state & 0x01);
  bool pio_b = (cmd_mask & 0x02) ? (cmd_state) : (asis_state & 0x02);

  uint32_t new_state = (!pio_a << 5) | (!pio_b << 6) | 0xf;

  uint8_t dev_cmd[] = {
      0x55,                                           // byte 0: match rom_code
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // byte 1-8: rom
      0x55,                                           // byte 9: write status
      0x07, 0x00, // byte:10-11: address of status byte
      0x00,       // byte 12: status byte to send (populated below)
      0x00, 0x00  // byte 13-14: crc16
  };

  size_t dev_cmd_size = sizeof(dev_cmd);
  int crc16_idx = dev_cmd_size - 2;
  dev_cmd[12] = new_state;

  dev->copyAddrToCmd(dev_cmd);

  // send the device the command excluding the crc16 bytes
  owb_s = owb_write_bytes(_ds, dev_cmd, dev_cmd_size - 2);

  if (owb_s != OWB_STATUS_OK) {

    Text::rlog("device \"%s\" send read cmd failure owb_s=\"%d\"", dev->id(),
               owb_s);

    return rc;
  }

  // device sends back the crc16 of the transmittd data (all bytes)
  // so, read just the two crc16 bytes into the dev_cmd
  owb_s = owb_read_bytes(_ds, (dev_cmd + crc16_idx), 2);

  if (owb_s != OWB_STATUS_OK) {

    Text::rlog("device \"%s\" failed to read cmd results owb_s=\"%d\"",
               dev->id(), owb_s);

    return rc;
  }

  bool crc16 = check_crc16((dev_cmd + 9), 4, &dev_cmd[crc16_idx]);

  if (crc16) {
    // writing 0xFF persists scratchpad to status
    owb_s = owb_write_byte(_ds, 0xff);

    if (owb_s != OWB_STATUS_OK) {

      Text::rlog("device \"%s\" failed to persist scratchpad owb_s=\"%d\"",
                 dev->id(), owb_s);

      return rc;
    }

    resetBus();
    rc = true;
  } else {

    Text::rlog("device \"%s\" failed crc16=\"0x%02x\"", dev->id(), crc16);
  }

  return rc;
}

bool DallasSemi::setDS2408(DsDevice_t *dev, uint32_t cmd_mask,
                           uint32_t cmd_state) {
  owb_status owb_s;
  bool rc = false;

  // read the device to ensure we have the current state
  // important because setting the new state relies, in part, on the existing
  // state for the pios not changing
  if (readDevice(dev) == false) {
    Text::rlog("device \"%s\" read before set failed", dev->id());

    return rc;
  }

  Positions_t *reading = (Positions_t *)dev->reading();

  uint32_t asis_state = reading->state();
  uint32_t new_state = 0x00;

  // use XOR tricks to apply the state changes to the as_is state using the
  // mask computed
  new_state = asis_state ^ ((asis_state ^ cmd_state) & cmd_mask);

  // report_state = new_state;
  new_state = (~new_state) & 0xFF; // constrain to 8-bits

  resetBus();

  uint8_t dev_cmd[] = {0x55, // byte 0: match rom_code
                       0x00, // byte 1-8: rom
                       0x00,
                       0x00,
                       0x00,
                       0x00,
                       0x00,
                       0x00,
                       0x00,
                       0x5a,                 // byte 9: actual device cmd
                       (uint8_t)new_state,   // byte10 : new state
                       (uint8_t)~new_state}; // byte11: inverted state

  dev->copyAddrToCmd(dev_cmd);
  owb_s = owb_write_bytes(_ds, dev_cmd, sizeof(dev_cmd));

  if (owb_s != OWB_STATUS_OK) {
    Text::rlog("device \"%s\" set cmd failed owb_s=\"%d\"", dev->debug().get(),
               owb_s);

    return rc;
  }

  uint8_t check[2];
  // read the confirmation byte (0xAA) and new state
  owb_s = owb_read_bytes(_ds, check, sizeof(check));

  if (owb_s != OWB_STATUS_OK) {

    Text::rlog("device \"%s\" check byte read failed owb_s=\"%d\"",
               dev->debug().get(), owb_s);

    return rc;
  }

  resetBus();

  // check what the device returned to determine success or failure
  // byte 0: 0xAA is the confirmation response
  // byte 1: new_state
  uint8_t conf_byte = check[0];
  uint8_t dev_state = check[1];
  if ((conf_byte == 0xaa) || (dev_state == new_state)) {
    rc = true;
  } else if (((conf_byte & 0xa0) == 0xa0) || ((conf_byte & 0x0a) == 0x0a)) {
    Text::rlog("device \"%s\" SET OK-PARTIAL conf=\"%02x\" state "
               "req=\"%02x\" dev=\"%02x\"",
               dev->id(), conf_byte, new_state, dev_state);
    rc = true;
  } else {
    Text::rlog("device \"%s\" SET FAILED conf=\"%02x\" state req=\"%02x\" "
               "dev=\"%02x\"",
               dev->id(), conf_byte, new_state, dev_state);
  }

  return rc;
}

bool DallasSemi::setDS2413(DsDevice_t *dev, uint32_t cmd_mask,
                           uint32_t cmd_state) {
  owb_status owb_s;
  bool rc = false;

  // read the device to ensure we have the current state
  // important because setting the new state relies, in part, on the existing
  // state for the pios not changing
  if (readDevice(dev) == false) {
    Text::rlog("device \"%s\" read before set failed", dev->id());

    return rc;
  }

  Positions_t *reading = (Positions_t *)dev->reading();

  uint32_t asis_state = reading->state();
  uint32_t new_state = 0x00;

  // use XOR tricks to apply the state changes to the as_is state using the
  // mask computed
  new_state = asis_state ^ ((asis_state ^ cmd_state) & cmd_mask);

  // report_state = new_state;
  new_state = (~new_state) & 0xFF; // constrain to 8-bits

  uint8_t dev_cmd[] = {0x55, // byte 0: match rom_code
                       0x00, // byte 1-8: rom
                       0x00,
                       0x00,
                       0x00,
                       0x00,
                       0x00,
                       0x00,
                       0x00,
                       0x5a,                 // byte 9: PIO Access Write cmd
                       (uint8_t)new_state,   // byte10 : new state
                       (uint8_t)~new_state}; // byte11: inverted state

  dev->copyAddrToCmd(dev_cmd);
  owb_s = owb_write_bytes(_ds, dev_cmd, sizeof(dev_cmd));

  if (owb_s != OWB_STATUS_OK) {
    return rc;
  }

  uint8_t check[2] = {0x00};
  owb_s = owb_read_bytes(_ds, check, sizeof(check));

  if (owb_s != OWB_STATUS_OK) {
    return rc;
  }

  resetBus();

  // check what the device returned to determine success or failure
  // byte 0 = 0xAA is a success, byte 1 = new_state
  // this might be a bit of a hack however let's accept success if either
  // the check byte is 0xAA *OR* the reported dev_state == new_state
  // this handles the occasional situation where there is a single dropped
  // bit in either (but hopefully not both)
  uint32_t dev_state = check[1];
  if ((check[0] == 0xaa) || (dev_state == (new_state & 0xff))) {
    rc = true;
  }

  return rc;
}

bool DallasSemi::check_crc16(const uint8_t *input, uint16_t len,
                             const uint8_t *inverted_crc, uint16_t crc) {
  crc = ~crc16(input, len, crc);
  return (crc & 0xFF) == inverted_crc[0] && (crc >> 8) == inverted_crc[1];
}

uint16_t DallasSemi::crc16(const uint8_t *input, uint16_t len, uint16_t crc) {
  static const uint8_t oddparity[16] = {0, 1, 1, 0, 1, 0, 0, 1,
                                        1, 0, 0, 1, 0, 1, 1, 0};

  for (uint16_t i = 0; i < len; i++) {
    // Even though we're just copying a byte from the input,
    // we'll be doing 16-bit computation with it.
    uint16_t cdata = input[i];
    cdata = (cdata ^ crc) & 0xff;
    crc >>= 8;

    if (oddparity[cdata & 0x0F] ^ oddparity[cdata >> 4])
      crc ^= 0xC001;

    cdata <<= 6;
    crc ^= cdata;
    cdata <<= 1;
    crc ^= cdata;
  }

  return crc;
}

} // namespace ruth
