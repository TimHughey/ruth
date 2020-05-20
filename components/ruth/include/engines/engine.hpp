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
#include "engines/types.hpp"
#include "misc/elapsedMillis.hpp"
#include "misc/local_types.hpp"
#include "misc/nvs.hpp"
#include "misc/profile.hpp"
#include "misc/restart.hpp"
#include "net/network.hpp"
#include "protocols/mqtt.hpp"
#include "readings/readings.hpp"

namespace ruth {

using std::vector;

template <class DEV> class Engine {

  typedef vector<DEV *> DeviceMap_t;

private:
  TaskMap_t _task_map;
  DeviceMap_t _devices;

  EventGroupHandle_t _evg;
  SemaphoreHandle_t _bus_mutex = nullptr;

  engineEventBits_t _event_bits = {.need_bus = BIT0,
                                   .engine_running = BIT1,
                                   .devices_available = BIT2,
                                   .temp_available = BIT3,
                                   .temp_sensors_available = BIT4};

  //
  // Core Task Implementation
  //
  // to satisify C++ requiremeents we must wrap the object member function
  // in a static function
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

  EngineTask_t *lookupTaskData(TaskTypes_t task_type) {
    using std::find_if;

    // my first lambda in C++, wow this languge has really evolved
    // since I used it 15+ years ago
    auto task = find_if(_task_map.begin(), _task_map.end(),
                        [task_type](EngineTask_t *search) {
                          return search->type() == task_type;
                        });

    if (task != _task_map.end()) {
      return *task;
    }

    return nullptr;
  }

  //
  // Default Do Nothing Task Implementation
  //
  void doNothing() {
    while (true) {
      vTaskDelay(pdMS_TO_TICKS(60 * 1000));
    }
  }

public:
  Engine() {
    _evg = xEventGroupCreate();
    _bus_mutex = xSemaphoreCreateMutex();
  };

  virtual ~Engine(){};

  // task methods
  void addTask(const string_t &engine_name, EngineTask_t &task) {

    EngineTask_ptr_t new_task = new EngineTask(task);
    // when the task name is 'core' then set it to the engine name
    // otherwise prepend the engine name
    if (new_task->_name.find("core") != string_t::npos) {
      new_task->_name = engine_name;
    } else {
      new_task->_name.insert(0, engine_name);
    }

    _task_map.push_back(new_task);
  }

  void saveTaskLastWake(TaskTypes_t tt) {
    auto task = lookupTaskData(tt);
    task->_lastWake = xTaskGetTickCount();
  }

  void taskDelayUntil(TaskTypes_t tt, TickType_t ticks) {
    auto task = lookupTaskData(tt);

    ::vTaskDelayUntil(&(task->_lastWake), ticks);
  }

  xTaskHandle taskHandle() {

    auto *task = lookupTaskData(TASK_CORE);

    if (task) {
      return task->handle();
    }

    return nullptr;
  }

  void delay(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

  virtual void core(void *data) = 0; // pure virtual, subclass must implement

  void start(void *task_data = nullptr) {
    // we make an assumption that the 'core' task was added
    auto task = lookupTaskData(TASK_CORE);

    if (task && task->handle()) {
      // already running
      return;
    }

    // this (object) is passed as the data to the task creation and is
    // used by the static runCore method to call the implemented run
    // method
    xTaskCreate(&runCore, task->_name.c_str(), task->_stackSize, this,
                task->_priority, &task->_handle);

    // now start any sub-tasks added
    for_each(_task_map.begin(), _task_map.end(), [this](EngineTask_t *task) {
      auto task_type = task->type();
      TaskFunc_t *run_task;

      switch (task_type) {
      case TASK_CORE:
        // core is already started, skip it
        task = nullptr;

        break;
      case TASK_CONVERT:
        run_task = &runConvert;

        break;
      case TASK_DISCOVER:
        run_task = &runDiscover;

        break;
      case TASK_COMMAND:
        run_task = &runCommand;

        break;
      case TASK_REPORT:
        run_task = &runReport;

        break;
      default:
        task = nullptr;
      }

      if (task) {
        ::xTaskCreate(run_task, task->_name.c_str(), task->_stackSize, this,
                      task->_priority, &task->_handle);
      }
    });
  }

  static uint32_t maxDevices() { return 100; };

  bool any_of_devices(bool (*func)(const DEV &)) {
    using std::any_of;

    return any_of(_devices.cbegin(), _devices.cend(), func);
  }

  // justSeenDevice():
  //    looks in known devices for the device.  if found, calls justSeen() on
  //    the device and returns it
  //
  DEV *justSeenDevice(DEV &dev) {
    auto *found_dev = findDevice(dev.id());

    if (found_dev) {
      auto was_missing = found_dev->missing();

      found_dev->justSeen();

      if (was_missing) {
        textReading_t *rlog = new textReading();
        textReading_ptr_t rlog_ptr(rlog);

        rlog->printf("missing device \"%s\" has returned",
                     found_dev->id().c_str());
        rlog->publish();
      }
    }

    return found_dev;
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

  void logSubTaskStart(void *task_info) {
    logSubTaskStart((EngineTask_ptr_t)task_info);
  }

  void logSubTaskStart(EngineTask_ptr_t task_info) {}

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

  void setCmdAck(DEV *dev, const char *refid, elapsedMicros latency_us) {
    if (dev) {
      dev->setReadingCmdAck(latency_us, refid);
    }
  }

  //
  // Command Queue
  //
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

  bool _queuePayload(MsgPayload_t *payload) {
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
      q_rc = xQueueSendToBack(_cmd_q, (void *)&payload, pdMS_TO_TICKS(10));

      if (q_rc == pdTRUE) {
        rc = true;
      } else {
        // payload was not queued, delete it
        delete payload;
      }
    }
    return rc;
  }
};
} // namespace ruth

#endif
