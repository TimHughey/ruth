/*
     engine.hpp - Ruth Dallas Semiconductor
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

#ifndef _ruth_engine_h_
#define _ruth_engine_h_

#include <algorithm>
#include <cstdlib>
#include <vector>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "devs/base.hpp"
#include "engines/event_bits.hpp"
#include "engines/task.hpp"
#include "misc/elapsedMillis.hpp"
#include "local/types.hpp"
#include "misc/nvs.hpp"
#include "misc/restart.hpp"
#include "net/network.hpp"
#include "net/profile/profile.hpp"
#include "protocols/mqtt.hpp"
#include "readings/readings.hpp"

namespace ruth {

using std::vector;

template <class DEV> class Engine {

protected:
  // Device Map
  typedef vector<DEV *> DeviceMap_t;

  // subclasses are permitted to use a read only reference to the
  // the device map managed by Engine
  const DeviceMap_t &deviceMap() const { return _devices; }

public:
  Engine(EngineTypes_t engine_type) : _engine_type(engine_type) {
    _evg = xEventGroupCreate();
    _bus_mutex = xSemaphoreCreateMutex();
  };

  virtual ~Engine(){};

  static uint32_t maxDevices() { return 100; };

  // justSeenDevice():
  //    looks in known devices for the device.  if found, calls justSeen() on
  //    the device and returns it
  //
  DEV *justSeenDevice(DEV &dev) {
    auto *found_dev = findDevice(dev.id());

    justSeenDevice(found_dev);

    return found_dev;
  };

  // justSeenDevice():
  //    looks in known devices for the device.  if found, calls justSeen() on
  //    the device and returns it
  //
  DEV *justSeenDevice(DEV *dev) {

    if (dev == nullptr) {
      return dev;
    }

    auto was_missing = dev->missing();

    dev->justSeen();

    if (was_missing) {
      textReading_t *rlog = new textReading();
      textReading_ptr_t rlog_ptr(rlog);

      rlog->printf("missing device \"%s\" has returned", dev->id().c_str());
      rlog->publish();
    }

    return dev;
  };

  bool addDevice(DEV *dev) {
    auto rc = false;
    DEV *found = nullptr;

    if (numKnownDevices() > maxDevices()) {
      textReading_t *rlog = new textReading();
      textReading_ptr_t rlog_ptr(rlog);

      rlog->printf("adding device \"%s\" would exceed max devices",
                   dev->id().c_str());
      rlog->publish();

      return rc;
    }

    if ((found = findDevice(dev->id())) == nullptr) {
      dev->justSeen();
      _devices.push_back(dev);
    }

    return (found == nullptr) ? true : false;
  };

  DEV *findDevice(const string_t &dev) {
    using std::find_if;

    // my first lambda in C++, wow this languge has really evolved
    // since I used it 15+ years ago
    auto found = find_if(_devices.begin(), _devices.end(),
                         [dev](DEV *search) { return search->id() == dev; });

    if (found != _devices.end()) {
      return *found;
    }

    return nullptr;
  }

  auto beginDevices() -> typename DeviceMap_t::iterator {
    return _devices.begin();
  }

  auto endDevices() -> typename DeviceMap_t::iterator { return _devices.end(); }

  auto knownDevices() -> typename DeviceMap_t::iterator {
    return _devices.begin();
  }
  bool endOfDevices(typename DeviceMap_t::iterator it) {
    return it == _devices.end();
  };

  bool moreDevices(typename DeviceMap_t::iterator it) {
    return it != _devices.end();
  };

  uint32_t numKnownDevices() { return _devices.size(); };
  bool isDeviceKnown(const string_t &id) {
    bool rc = false;

    rc = (findDevice(id) == nullptr ? false : true);
    return rc;
  };

protected:
  virtual void convert(void *data) { doNothing(); };
  virtual void command(void *data) { doNothing(); };
  virtual void discover(void *data) { doNothing(); };
  virtual void report(void *data) { doNothing(); };

  virtual bool readDevice(DEV *dev) = 0;

  // event group bits
  EventBits_t engineBit() { return _event_bits.engine_running; }
  EventBits_t needBusBit() { return _event_bits.need_bus; }
  EventBits_t devicesAvailableBit() { return _event_bits.devices_available; }
  EventBits_t devicesOrTempSensorsBit() {
    return (_event_bits.devices_available | _event_bits.temp_sensors_available);
  }
  EventBits_t tempSensorsAvailableBit() {
    return _event_bits.temp_sensors_available;
  }
  EventBits_t temperatureAvailableBit() { return _event_bits.temp_available; }

  // event group
  void devicesAvailable(bool available = true) {
    if (available) {
      xEventGroupSetBits(_evg, _event_bits.devices_available);
    } else {
      xEventGroupClearBits(_evg, _event_bits.devices_available);
    }
  }
  void devicesUnavailable() {
    xEventGroupClearBits(_evg, _event_bits.devices_available);
  }

  void engineRunning() { xEventGroupSetBits(_evg, _event_bits.engine_running); }
  bool isBusNeeded() {
    EventBits_t bits = xEventGroupGetBits(_evg);
    return (bits & needBusBit());
  }

  void needBus() { xEventGroupSetBits(_evg, needBusBit()); }
  void clearNeedBus() { xEventGroupClearBits(_evg, needBusBit()); }
  void tempAvailable() { xEventGroupSetBits(_evg, _event_bits.temp_available); }
  void tempUnavailable() {
    xEventGroupClearBits(_evg, _event_bits.temp_available);
  }
  void temperatureSensors(bool available) {
    if (available) {
      xEventGroupSetBits(_evg, _event_bits.temp_sensors_available);
    } else {
      xEventGroupClearBits(_evg, _event_bits.temp_sensors_available);
    }
  }

  EventBits_t waitFor(EventBits_t bits, TickType_t wait_ticks = portMAX_DELAY,
                      bool clear_bits = false) {
    EventBits_t set_bits = xEventGroupWaitBits(
        _evg, bits,
        (clear_bits ? pdTRUE : pdFALSE), // clear bits (if set while waiting)
        pdTRUE, // wait for all bits, not really needed here
        wait_ticks);

    return (set_bits);
  }

  bool waitForEngine() {
    EventBits_t b = engineBit();
    EventBits_t sb;

    sb = xEventGroupWaitBits(_evg, b, // bit to wait for
                             pdFALSE, // don't clear bit
                             pdTRUE,  // wait for all bits
                             portMAX_DELAY);

    return (sb & b);
  }

  // semaphore
  void giveBus() { xSemaphoreGive(_bus_mutex); }
  bool takeBus(TickType_t wait_ticks = portMAX_DELAY) {
    // the bus will be in an indeterminate state if we do acquire it so
    // call resetBus(). said differently, we could have taken the bus
    // in the middle of some other operation (e.g. discover, device read)
    if ((xSemaphoreTake(_bus_mutex, wait_ticks) == pdTRUE) && resetBus()) {
      return true;
    }

    return false;
  }

  bool publish(const string_t &dev_id) {
    DEV *search = findDevice(dev_id);

    if (search != nullptr) {
      return publish(search);
    }

    return false;
  };

  bool publish(DEV *dev) {
    bool rc = true;

    if (dev != nullptr) {
      Reading_t *reading = dev->reading();

      if (reading != nullptr) {
        publish(reading);
        rc = true;
      }
    }
    return rc;
  };

  bool publish(Reading_t *reading) {
    auto rc = false;

    if (reading) {
      MQTT::publish(reading);
    }

    return rc;
  };

  virtual bool resetBus(bool *additional_status = nullptr) { return true; }

  //////
  //
  // Command Functionality
  //
  //////

protected:
  const int _max_queue_depth = 30;
  QueueHandle_t _cmd_q = nullptr;
  elapsedMicros _cmd_elapsed;
  elapsedMicros _latency_us;

  bool commandAck(DEV *dev, bool ack, const string_t &refid) {
    bool rc = false;

    if (dev != nullptr) {
      rc = readDevice(dev);

      if (rc && ack) {
        dev->setReadingCmdAck(_cmd_elapsed, refid);
        publish(dev);
      }
    }

    return rc;
  }

  virtual bool commandExecute(DEV *dev, uint32_t cmd_mask, uint32_t cmd_state,
                              bool ack, const string_t &refif,
                              elapsedMicros &cmd_elapsed) {
    return false;
  }

  void setCmdAck(DEV *dev, const char *refid, elapsedMicros latency_us) {
    if (dev) {
      dev->setReadingCmdAck(latency_us, refid);
    }
  }

  bool _queuePayload(MsgPayload_t_ptr payload_ptr) {
    auto rc = false;
    auto q_rc = pdTRUE;

    if (uxQueueSpacesAvailable(_cmd_q) == 0) {
      MsgPayload_t *old = nullptr;

      q_rc = xQueueReceive(_cmd_q, &old, pdMS_TO_TICKS(10));

      if ((q_rc == pdTRUE) && (old != nullptr)) {
        delete old;
      }
    }

    if (q_rc == pdTRUE) {
      MsgPayload_t *payload = payload_ptr.get();
      q_rc = xQueueSendToBack(_cmd_q, (void *)&payload, pdMS_TO_TICKS(10));

      if (q_rc == pdTRUE) {
        // since the payload was queued and we need to keep it until processed
        // release it from the unique ptr
        payload_ptr.release();
        rc = true;
      }
    }
    return rc;
  }

  //////
  //
  // Protected Engine Tasks API
  //
  //////

protected:
  void addTask(const EngineTaskTypes_t task_type) {
    EngineTask_t *new_task = nullptr;

    switch (task_type) {
    case TASK_CORE:
      new_task = new EngineTask(_engine_type, task_type, &runCore);
      break;

    case TASK_COMMAND:
      new_task = new EngineTask(_engine_type, task_type, &runCommand);
      break;

    case TASK_DISCOVER:
      new_task = new EngineTask(_engine_type, task_type, &runDiscover);
      break;

    case TASK_REPORT:
      new_task = new EngineTask(_engine_type, task_type, &runReport);
      break;

    case TASK_CONVERT:
      new_task = new EngineTask(_engine_type, task_type, &runConvert);
      break;

    case TASK_END_OF_LIST:
      break;
    }

    if (new_task) {
      // ESP_LOGI("ENGINE", "adding task \"%s\"", new_task->name_cstr());
      _cache_task_data[task_type] = new_task;
      _task_map.push_back(new_task);
    }
  }

  void saveTaskLastWake(EngineTaskTypes_t tt) {
    EngineTask_t *task = lookupTaskData(tt);
    TickType_t *last_wake = task->lastWake_ptr();

    *last_wake = xTaskGetTickCount();
  }

  void taskDelayUntil(EngineTaskTypes_t tt, TickType_t ticks) {
    auto task = lookupTaskData(tt);

    vTaskDelayUntil(task->lastWake_ptr(), ticks);
  }

  void delay(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

  virtual void core(void *data) = 0; // pure virtual, subclass must implement

  void start(void *task_data = nullptr) {

    // now start any sub-tasks added
    for_each(_task_map.begin(), _task_map.end(), [this](EngineTask_t *task) {
      if (task && task->handleNull()) {
        if (_cache_task_data[task->type()] == nullptr) {
          _cache_task_data[task->type()] = task;
        }

        // ESP_LOGI("ENGINE", "starting \"%s\" stack=%d", task->name_cstr(),
        //          task->stackSize());
        xTaskCreate(task->taskFunc(), task->name_cstr(), task->stackSize(),
                    this, task->priority(), task->handle_ptr());
      }
    });
  }

  //////
  //
  // Private Engine Task Functions
  //
  //////
private:
  void buildTaskName(EngineTaskTypes_t task_type, string_t &task_name);

  //////
  //
  // Private Engine Core Member Varibles
  //
  //////

private:
  string_t _engine_name;
  EngineTypes_t _engine_type;
  TaskMap_t _task_map;
  DeviceMap_t _devices;
  EngineTask_t *_cache_task_data[TASK_END_OF_LIST] = {nullptr};

  //////
  //
  // Private Engine Task Functionality
  //
  //////

private:
  EventGroupHandle_t _evg;
  SemaphoreHandle_t _bus_mutex = nullptr;

  engineEventBits_t _event_bits = {.need_bus = BIT0,
                                   .engine_running = BIT1,
                                   .devices_available = BIT2,
                                   .temp_available = BIT3,
                                   .temp_sensors_available = BIT4};

  EngineTask_t *lookupTaskData(EngineTaskTypes_t task_type) {
    using std::find_if;

    EngineTask_t *cached_task = _cache_task_data[task_type];

    // avoid the search for_each if we already know the EngineTask_t
    // saving cpu cycles and stack size
    if (cached_task) {
      return cached_task;
    }

    // if not in cache, look it up

    // my first lambda in C++, wow this languge has really evolved
    // since I used it 15+ years ago
    auto task = find_if(_task_map.begin(), _task_map.end(),
                        [task_type](EngineTask_t *search) {
                          return (search) && (search->type() == task_type);
                        });

    if (task != _task_map.end()) {
      ESP_LOGI("ENGINE", "cache miss for \"%s\" task data",
               (*task)->name_cstr());

      _cache_task_data[task_type] = *task;

      return *task;
    } else {
      ESP_LOGW("ENGINE", "lookupTaskData could not find task_type=%d",
               task_type);
    }

    return nullptr;
  }

  //
  // Core Task Implementation
  //

  // NOTE:
  //  to satisify C++ requiremeents we must wrap the object member function
  //  in a static function
  //
  static void runCore(void *instance) {
    Engine *task_instance = (Engine *)instance;
    auto *task = task_instance->lookupTaskData(TASK_CORE);

    if (task_instance && task) {
      task_instance->core(task->data());
    }
  }

  //
  // Engine Sub Tasks
  //
  static void runConvert(void *instance) {
    Engine *task_instance = (Engine *)instance;
    auto *task = task_instance->lookupTaskData(TASK_CONVERT);

    if (task_instance && task) {
      task_instance->convert(task->data());
    }
  }

  static void runDiscover(void *instance) {
    Engine *task_instance = (Engine *)instance;
    auto *task = task_instance->lookupTaskData(TASK_DISCOVER);

    if (task_instance && task) {
      task_instance->discover(task->data());
    }
  }

  static void runReport(void *instance) {
    Engine *task_instance = (Engine *)instance;
    auto *task = task_instance->lookupTaskData(TASK_REPORT);

    if (task_instance && task) {
      task_instance->report(task->data());
    }
  }

  static void runCommand(void *instance) {
    Engine *task_instance = (Engine *)instance;
    auto *task = task_instance->lookupTaskData(TASK_COMMAND);

    if (task_instance && task) {
      task_instance->command(task->data());
    }
  }

  //
  // Default Do Nothing Task Implementation
  //
  void doNothing() {
    while (true) {
      vTaskDelay(pdMS_TO_TICKS(60 * 1000));
    }
  }
};
} // namespace ruth

#endif
