/*
     engines/task_map.hpp - Ruth Engine Task Tracking Map
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

#ifndef _ruth_engine_task_map_hpp
#define _ruth_engine_task_map_hpp

#include <algorithm>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "engines/task.hpp"
#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "net/profile/profile.hpp"

namespace ruth {

typedef class EngineTaskMap EngineTaskMap_t;
class EngineTaskMap {
public:
  EngineTaskMap() {}

  void add(const EngineTask_t &task) { _tasks[task.type()] = task; }
  void notify(EngineTaskTypes_t engine_type) { _tasks[engine_type].notify(); }
  void notifyClear(EngineTaskTypes_t engine_type) {
    _tasks[engine_type].notifyClear();
  }
  EngineTask_t &get(int type) { return _tasks[type]; }

private:
  EngineTask_t _tasks[EngineTaskTypes_t::TASK_END_OF_LIST];
};

} // namespace ruth

#endif
