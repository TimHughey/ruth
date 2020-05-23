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

#ifndef engine_task_hpp
#define engine_task_hpp

#include <algorithm>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "local/types.hpp"
#include "misc/elapsedMillis.hpp"
#include "net/profile/profile.hpp"

namespace ruth {

using std::vector;

typedef class EngineTask EngineTask_t;
typedef EngineTask_t *EngineTask_ptr_t;

typedef vector<EngineTask_t *> TaskMap_t;

typedef TaskMap_t *TaskMap_ptr_t;

class EngineTask {
public:
  // create a new EngineTask with settings from Profile
  EngineTask(EngineTypes_t engine_type, EngineTaskTypes_t task_type,
             TaskFunc_t *task_func, void *data = nullptr)
      : _engine_type(engine_type), _task_type(task_type) {

    _task_func = task_func;
    _data = data;

    _priority = Profile::engineTaskPriority(engine_type, task_type);
    _stack_size = Profile::engineTaskStack(engine_type, task_type);

    assembleName();
  }

  void *data() { return _data; }
  TaskHandle_t &handle() { return _handle; }
  bool handleNull() { return (_handle == nullptr) ? true : false; }
  TaskHandle_t *handle_ptr() { return &_handle; }
  TickType_t &lastWake() { return _last_wake; }
  TickType_t *lastWake_ptr() { return &_last_wake; }
  const char *name_cstr() const { return _name.c_str(); }
  const string_t &name() const { return _name; }
  UBaseType_t stackSize() const { return _stack_size; }
  TaskFunc_t const *taskFunc() const { return _task_func; }
  UBaseType_t priority() const { return _priority; }
  EngineTaskTypes_t type() { return _task_type; }

private:
  void assembleName();

private:
  EngineTypes_t _engine_type;
  EngineTaskTypes_t _task_type;
  TaskHandle_t _handle = nullptr;
  TaskFunc_t *_task_func = nullptr;
  TickType_t _last_wake = 0;
  UBaseType_t _stack_size = 0;
  UBaseType_t _priority = 0;
  string_t _name = "unamed";
  void *_data;
};

} // namespace ruth
#endif // engines_types_h
