/*
     engines/task.cpp - Ruth Engine Task Info
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

#include "engines/task.hpp"

namespace ruth {

void EngineTask::assembleName() {
  static const char *base[ENGINE_END_OF_LIST] = {"DLS", "I2C", "PWM"};

  switch (_task_type) {
  case TASK_CORE:
    _name = base[_engine_type];
    break;

  case TASK_REPORT:
    _name.printf("%s-rpt", base[_engine_type]);
    break;

  case TASK_COMMAND:
    _name.printf("%s-cmd", base[_engine_type]);
    break;

  case TASK_END_OF_LIST:
    break;
  }
}

void EngineTask::notify() const {
  if (_handle) {
    xTaskNotify(_handle, 0, eIncrement);
  }
}

void EngineTask::notifyClear() const {
  if (_handle) {
    xTaskNotify(_handle, 0, eSetValueWithOverwrite);
  }
}

} // namespace ruth
