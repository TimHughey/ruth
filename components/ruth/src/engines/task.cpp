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

  _name = base[_engine_type];

  switch (_task_type) {
  case TASK_CORE:
    break;

  case TASK_CONVERT:
    _name.append("-cvt");
    break;

  case TASK_DISCOVER:
    _name.append("-dsc");
    break;

  case TASK_REPORT:
    _name.append("-rpt");
    break;

  case TASK_COMMAND:
    _name.append("-cmd");

  case TASK_END_OF_LIST:
    break;
  }
}

} // namespace ruth
