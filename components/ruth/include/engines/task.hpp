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

#ifndef _ruth_engine_task_hpp
#define _ruth_engine_task_hpp

#include <algorithm>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "net/profile/profile.hpp"

namespace ruth {

typedef class EngineTask EngineTask_t;

class EngineTask {
public:
  EngineTask() {}
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
  const char *name() const { return _name.c_str(); }
  size_t nameMaxLength() const { return (CONFIG_FREERTOS_MAX_TASK_NAME_LEN); }
  UBaseType_t priority() const { return _priority; }
  UBaseType_t stackSize() const { return _stack_size; }
  TaskFunc_t const *taskFunc() const { return _task_func; }
  EngineTaskTypes_t type() const { return _task_type; }
  bool valid() const { return (_task_func == nullptr) ? false : true; }

private:
  void assembleName();

private:
  EngineTypes_t _engine_type = ENGINE_END_OF_LIST;
  EngineTaskTypes_t _task_type = TASK_END_OF_LIST;
  TaskHandle_t _handle = nullptr;
  TaskFunc_t *_task_func = nullptr;
  UBaseType_t _stack_size = 0;
  UBaseType_t _priority = 0;
  TaskName_t _name;
  void *_data = nullptr;
};

} // namespace ruth
#endif // engines_types_h
