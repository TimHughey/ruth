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

#ifndef _ruth_engine_hpp
#define _ruth_engine_hpp

#include <algorithm>
#include <cstdlib>
#include <vector>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "devs/base/base.hpp"
#include "engines/event_bits.hpp"
#include "engines/task.hpp"
#include "engines/task_map.hpp"
#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "misc/restart.hpp"
#include "net/network.hpp"
#include "net/profile/profile.hpp"
#include "protocols/mqtt.hpp"
#include "readings/readings.hpp"

namespace ruth {
using std::vector;
using namespace reading;

template <class DEV, EngineTypes_t ENGINE_TYPE> class Engine {

protected:
  // Device Map
  typedef vector<DEV *> DeviceMap_t;

  // subclasses are permitted to use a read only reference to the
  // the device map managed by Engine
  inline const DeviceMap_t &deviceMap() const { return _devices; }

public:
  Engine() {
    _engine_type = ENGINE_TYPE;
    _bus_mutex = xSemaphoreCreateMutex();
  };

  virtual ~Engine(){};

  inline uint32_t defaultMissingSeconds() {
    return Profile::engineTaskInterval(ENGINE_TYPE, TASK_REPORT) * 1.5;
  }

  static uint32_t maxDevices() { return 35; };

  // justSeenDevice():
  //    looks in known devices for the device.  if found, calls justSeen() on
  //    the device and returns it
  //
  DEV *justSeenDevice(DEV &dev) {
    auto *found_dev = findDevice(dev.id());

    if (found_dev) {
      found_dev->justSeen();
    }

    return found_dev;
  };

  DEV *justSaw(DEV &d) {
    DEV *known_dev = nullptr;

    auto found = find_if(_devices.begin(), _devices.end(),
                         [d](const DEV *search) { return *search == d; });

    if (found != _devices.end()) {
      known_dev = *found;

      known_dev->justSeen();
    }

    return known_dev;
  }

  // justSeenDevice():
  //    looks in known devices for the device.  if found, calls justSeen() on
  //    the device and returns it
  //
  DEV *justSeenDevice(DEV *dev) {

    if (dev == nullptr) {
      return dev;
    }

    dev->justSeen();

    return dev;
  };

  bool addDevice(DEV *dev) {
    auto rc = false;
    DEV *found = nullptr;

    if (numKnownDevices() > maxDevices()) {
      Text::rlog("adding device \"%s\" would exceed max devices", dev->id());

      return rc;
    }

    if ((found = findDevice(dev->id())) == nullptr) {
      dev->justSeen();
      _devices.push_back(dev);
    }

    return (found == nullptr) ? true : false;
  };

  DEV *findDevice(const char *dev) {
    using std::find_if;

    // my first lambda in C++, wow this languge has really evolved
    // since I used it 15+ years ago
    auto found = find_if(_devices.begin(), _devices.end(),
                         [dev](DEV *search) { return search->matchID(dev); });

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

  uint32_t numKnownDevices() { return _devices.size(); };

protected:
  virtual void command(void *data) { doNothing(); };
  virtual void report(void *data) { doNothing(); };
  virtual bool readDevice(DEV *dev) = 0;

  void notifyDevicesAvailable() {
    // only notify other tasks once
    static bool need_notification = true;

    if ((numKnownDevices() > 0) && need_notification) {
      need_notification = false;
      _task_map.notify(TASK_COMMAND);
      _task_map.notify(TASK_REPORT);
    }
  }

  bool isBusNeeded(uint32_t wait_ms = 0) {
    // BaseType_t xTaskNotifyWait(uint32_t ulBitsToClearOnEntry, uint32_t
    // ulBitsToClearOnExit, uint32_t *pulNotificationValue, TickType_t
    // xTicksToWait)
    //
    // uint32_t ulTaskNotifyTake(BaseType_t xClearCountOnExit, TickType_t
    // xTicksToWait)

    // when the bus is needed there will be a notification waiting for the task
    auto notified =
        xTaskNotifyWait(0x00, ULONG_MAX, nullptr, pdMS_TO_TICKS(wait_ms));

    return (notified == pdTRUE) ? true : false;

    // return ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms) > 0) ? true :
    // false;
  }

  void needBus() {
    _task_map.notify(TASK_CORE);
    _task_map.notify(TASK_REPORT);
  }

  void holdForDevicesAvailable() {
    elapsedMillis elapsed;
    // wait indefinitely for devices to become available, when notified clear
    // the notification value as direct to task notifications are reused for
    // forcibly taking the bus mutex
    uint32_t notify_val;
    xTaskNotifyWait(0x00, ULONG_MAX, &notify_val, portMAX_DELAY);

    Text::rlog("[%s] holdForDevicesAvailable() took %0.2fs",
               pcTaskGetTaskName(nullptr), (float)elapsed);
  }

  bool acquireBus(TickType_t wait_ticks = portMAX_DELAY) {
    return (xSemaphoreTake(_bus_mutex, wait_ticks) == pdTRUE) ? true : false;
  }

  // semaphore
  bool giveBus() {
    xSemaphoreGive(_bus_mutex);
    return true;
  }

  bool takeBus(TickType_t wait_ticks = portMAX_DELAY) {
    needBus();
    // the bus will be in an indeterminate state if we do acquire it so
    // call resetBus(). said differently, we could have taken the bus
    // in the middle of some other operation (e.g. discover, device read)
    if (xSemaphoreTake(_bus_mutex, wait_ticks) == pdTRUE) {
      return true;
    }

    return false;
  }

  void releaseBus() {
    _task_map.notifyClear(TASK_CORE);
    _task_map.notifyClear(TASK_REPORT);

    giveBus();
  }

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

  virtual bool resetBus() { return true; }
  virtual bool resetBus(bool &additional_status) { return true; }

  //////
  //
  // Task / Action Frequencies
  //
  //////

  // delay times
  inline TickType_t coreFrequency() {
    return Profile::engineTaskIntervalTicks(ENGINE_TYPE, TASK_CORE);
  }

  inline TickType_t reportFrequency() {
    return Profile::engineTaskIntervalTicks(ENGINE_TYPE, TASK_REPORT);
  }

  //////
  //
  // Command Functionality
  //
  //////

protected:
  const int _max_queue_depth = 6;
  QueueHandle_t _cmd_q = nullptr;
  elapsedMicros _cmd_elapsed;
  elapsedMicros _latency_us;

  bool commandAck(DEV *dev, bool ack, const RefID_t &refid,
                  bool set_rc = true) {
    bool rc = false;

    if (dev && set_rc) {
      rc = readDevice(dev);

      if (rc && ack) {
        dev->setReadingCmdAck(_cmd_elapsed, refid);
        publish(dev);
      }
    }

    return rc;
  }

  virtual bool commandExecute(DEV *dev, uint32_t cmd_mask, uint32_t cmd_state,
                              bool ack, const RefID_t &refid,
                              elapsedMicros &cmd_elapsed) {
    return false;
  }

  void setCmdAck(DEV *dev, const char *refid, elapsedMicros latency_us) {
    if (dev) {
      dev->setReadingCmdAck((uint32_t)latency_us, refid);
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
    TaskFunc_t *task_func = nullptr;

    switch (task_type) {
    case TASK_CORE:
      task_func = &runCore;
      break;

    case TASK_COMMAND:
      task_func = &runCommand;
      break;

    case TASK_REPORT:
      task_func = &runReport;
      break;

    default:
      task_func = nullptr;
    }

    if (task_func) {
      _task_map.add(EngineTask(_engine_type, task_type, task_func));
    }
  }

  void saveLastWake(TickType_t &last_wake) { last_wake = xTaskGetTickCount(); }

  void delayUntil(TickType_t &last_wake, TickType_t ticks) {
    vTaskDelayUntil(&last_wake, ticks);
  }

  void delay(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

  virtual void core(void *data) = 0; // pure virtual, subclass must implement

  void start(void *task_data = nullptr) {

    for (int type = TASK_CORE; type < TASK_END_OF_LIST; type++) {
      EngineTask_t &task = _task_map.get(type);

      xTaskCreate(task.taskFunc(), task.name(), task.stackSize(), this,
                  task.priority(), task.handle_ptr());
    }
  }

  //////
  //
  // Private Engine Core Member Varibles
  //
  //////

private:
  EngineTypes_t _engine_type;
  EngineTaskMap_t _task_map;
  DeviceMap_t _devices;

  //////
  //
  // Private Engine Task Functionality
  //
  //////

private:
  SemaphoreHandle_t _bus_mutex = nullptr;

  engineEventBits_t _event_bits = {.need_bus = BIT0,
                                   .engine_running = BIT1,
                                   .devices_available = BIT2,
                                   .temp_available = BIT3,
                                   .temp_sensors_available = BIT4};

  const EngineTask_t &lookupTaskData(EngineTaskTypes_t task_type) {
    return _task_map.get(task_type);
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

    if (task_instance) {
      auto task = task_instance->lookupTaskData(TASK_CORE);
      task_instance->core(task.data());
    }
  }

  TaskFunc_t const *coreFunction() { return &runCore; }

  //
  // Engine Sub Tasks
  //

  static void runReport(void *instance) {
    Engine *task_instance = (Engine *)instance;

    if (task_instance) {
      auto task = task_instance->lookupTaskData(TASK_REPORT);
      task_instance->report(task.data());
    }
  }

  static void runCommand(void *instance) {
    Engine *task_instance = (Engine *)instance;

    if (task_instance) {
      auto task = task_instance->lookupTaskData(TASK_COMMAND);
      task_instance->command(task.data());
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
