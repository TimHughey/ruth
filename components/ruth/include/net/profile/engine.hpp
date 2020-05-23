/*
    engine.hpp -- Engine Profile Information
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

#ifndef ruth_engine_profile_hpp
#define ruth_engine_profile_hpp

#include <algorithm>
#include <cstdlib>
#include <vector>

#include <esp_log.h>

#include "external/ArduinoJson.h"
#include "misc/local_types.hpp"
#include "net/profile/types.hpp"

namespace ruth {

using std::vector;

typedef class ProfileEngineTask ProfileEngineTask_t;

class ProfileEngineTask {
public:
  ProfileEngineTask(EngineTypes_t engine_type, EngineTaskTypes_t task_type,
                    const JsonObject engine_doc)
      : _engine_type(engine_type), _task_type(task_type) {

    _engine_key = lookupEngineKey(_engine_type);
    _task_key = lookupTaskKey(task_type);

    const JsonObject task_doc = engine_doc[_task_key];

    _stack_size = task_doc["stack"] | _stack_size;
    _priority = task_doc["pri"] | _priority;

    // the command task does not have a loop and thus no interval_ms
    if (task_type != TASK_COMMAND) {
      _interval_ms = task_doc["interval_ms"] | UINT32_MAX;
    }

    // ESP_LOGI("PET", "engine=\"%s\" task=\"%s\" stack=%u pri=%u
    // interval_ms=%u",
    //          _engine_key, _task_key, _stack_size, _priority, _interval_ms);
  }

  // DEFAULT DESTRUCTOR

  uint32_t intervalMS() const { return _interval_ms; }
  uint32_t priority() const { return _priority; }
  size_t stackSize() const { return _stack_size; }
  EngineTypes_t engineType() const { return _engine_type; }
  EngineTaskTypes_t taskType() const { return _task_type; }

  static const char *lookupEngineKey(EngineTypes_t engine_type) {
    static const char *engines[ENGINE_END_OF_LIST] = {"ds", "i2c", "pwm"};

    return engines[engine_type];
  }

  static const char *lookupTaskKey(EngineTaskTypes_t task_type) {
    static const char *tasks[TASK_END_OF_LIST] = {"core", "convert", "discover",
                                                  "report", "command"};

    return tasks[task_type];
  }

private:
  EngineTypes_t _engine_type;
  EngineTaskTypes_t _task_type;
  const char *_engine_key;
  const char *_task_key;
  size_t _stack_size = 4096;
  uint32_t _priority = 5;
  uint32_t _interval_ms = 0;
};
} // namespace ruth

#endif
