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
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace ruth {

// just in case we ever want to change
// TODO:  roll this out across the entire project
using string_t = std::string;

typedef std::unique_ptr<char[]> MsgBuff_t;

// type passed to xTaskCreate as the function to run as a task
typedef void(TaskFunc_t)(void *);

typedef struct {
  TaskHandle_t handle;
  void *data;
  TickType_t lastWake;
  UBaseType_t priority;
  UBaseType_t stackSize;
} Task_t;

typedef string_t RefID_t;

typedef std::vector<char> RawMsg_t;

} // namespace ruth
#endif // ruth_type_hpp
