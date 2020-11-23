/*
    local/types.hpp - Ruth Local Types
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

#ifndef _ruth_local_types_hpp
#define _ruth_local_types_hpp

#include <memory>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "misc/textbuffer.hpp"

namespace ruth {

typedef TextBuffer<20> Hostname_t;
typedef TextBuffer<128> OtaUri_t;
typedef TextBuffer<CONFIG_FREERTOS_MAX_TASK_NAME_LEN> TaskName_t;

// type passed to xTaskCreate as the function to run as a task
typedef void(TaskFunc_t)(void *);

typedef struct {
  TaskHandle_t handle;
  void *data;
  UBaseType_t priority;
  UBaseType_t stackSize;
} Task_t;

} // namespace ruth
#endif // ruth_type_hpp
