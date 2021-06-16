/*
    core.hpp - Ruth Core (C++ version of app_main())
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

#ifndef ruth_task_hpp
#define ruth_task_hpp

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ruth {

typedef struct {
  TaskHandle_t handle;
  void *data;
  UBaseType_t priority;
  UBaseType_t stackSize;
} Task_t;

} // namespace ruth

#endif
