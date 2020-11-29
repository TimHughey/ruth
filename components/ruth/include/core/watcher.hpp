/*
    core/watcher.hpp
    Ruth Core Watcher Task
    Copyright (C) 2020  Tim Hughey

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

#ifndef _ruth_core_watcher_hpp
#define _ruth_core_watcher_hpp

#include "external/ArduinoJson.h"
#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "net/network.hpp"
#include "protocols/mqtt.hpp"

namespace ruth {
typedef class Watcher Watcher_t;

class Watcher {
public:
  Watcher(uint32_t seconds = 5) : _seconds(seconds) {}
  ~Watcher() {
    if (_task.handle) {
      vTaskDelete(_task.handle);
    }
  }

  void start() {
    xTaskCreate(&runCore, "Watcher", _task.stackSize, this, _task.priority,
                &_task.handle);
  }

private:
  void core() {
    for (;;) {
      StaticJsonDocument<1740> doc;
      WatcherPayload_t msg_pack;
      UBaseType_t num_tasks = uxTaskGetNumberOfTasks();

      // UBaseType_t uxTaskGetSystemState(TaskStatus_t *constpxTaskStatusArray,
      // const UBaseType_t uxArraySize, uint32_t *constpulTotalRunTime)

      uxTaskGetSystemState(_sys_tasks, 30, nullptr);

      JsonObject root = doc.to<JsonObject>();

      root["host"] = Net::hostID();
      root["name"] = Net::hostname();
      root["mtime"] = time(nullptr);
      root["type"] = "watcher";

      // task_obj.createNestedArray("tasks");
      // JsonArray task_array = task_obj["tasks"];
      JsonArray task_array = root.createNestedArray("tasks");

      for (UBaseType_t i = 0; i < num_tasks; i++) {
        JsonObject info = task_array.createNestedObject();

        TaskStatus_t task = _sys_tasks[i];

        info["id"] = task.xTaskNumber;
        info["name"] = task.pcTaskName;
        info["stack_hw"] = task.usStackHighWaterMark;
      }

      JsonObject doc_stats = root.createNestedObject("doc_stats");
      doc_stats["capacity"] = doc.capacity();
      doc_stats["used"] = doc.memoryUsage();

      auto len = serializeMsgPack(doc, msg_pack.data(), msg_pack.capacity());
      msg_pack.forceSize(len);
      MQTT::publish(msg_pack);

      vTaskDelay(pdMS_TO_TICKS(_seconds * 1000));
    }
  }

  static void runCore(void *task_instance) {
    Watcher_t *task = (Watcher_t *)task_instance;
    task->core();
  }

private:
  uint32_t _seconds;
  TaskStatus_t _sys_tasks[30];

  Task_t _task = {.handle = nullptr,
                  .data = nullptr,
                  .priority = 1, // allow reporting to continue
                  .stackSize = (5 * 1024)};
};
} // namespace ruth

#endif
