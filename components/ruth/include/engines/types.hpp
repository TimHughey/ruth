/*
     engines/types.hpp - Ruth Engine Types
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

#ifndef engines_types_h
#define engines_types_h

#include <algorithm>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "misc/elapsedMillis.hpp"
#include "misc/local_types.hpp"
#include "misc/profile.hpp"

namespace ruth {

using std::vector;

typedef void(TaskFunc_t)(void *);

typedef enum {
  TASK_CORE,
  TASK_CONVERT,
  TASK_DISCOVER,
  TASK_REPORT,
  TASK_COMMAND
} TaskTypes_t;

typedef class EngineTask EngineTask_t;
typedef EngineTask_t *EngineTask_ptr_t;

typedef vector<EngineTask_t *> TaskMap_t;

typedef TaskMap_t *TaskMap_ptr_t;

class EngineTask {
public:
  // create a new EngineTask with settings from Profile
  EngineTask(TaskTypes_t task_type, char const *subsystem, char const *task,
             void *data = nullptr)
      : _task_type(task_type), _name(subsystem), _data(data) {
    _priority = Profile::subSystemTaskPriority(subsystem, task);
    _stackSize = Profile::subSystemTaskStack(subsystem, task);
  };

  void *data() { return _data; }
  TaskHandle_t handle() { return _handle; }
  TaskTypes_t type() { return _task_type; }

public:
  TaskTypes_t _task_type;
  string_t _name = "unamed";
  TaskHandle_t _handle = nullptr;
  TickType_t _lastWake = 0;
  UBaseType_t _priority = 0;
  UBaseType_t _stackSize = 0;
  void *_data = nullptr;
};

typedef struct {
  EventBits_t need_bus;
  EventBits_t engine_running;
  EventBits_t devices_available;
  EventBits_t temp_available;
  EventBits_t temp_sensors_available;
} engineEventBits_t;
} // namespace ruth
#endif // engines_types_h
