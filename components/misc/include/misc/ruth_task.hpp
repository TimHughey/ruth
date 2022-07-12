/*
    Ruth
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

#pragma once

#include <config.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ruth {

constexpr size_t task_max_name_len = CONFIG_FREERTOS_MAX_TASK_NAME_LEN;

typedef struct {
  TaskHandle_t handle;
  void *data;
  UBaseType_t priority;
  UBaseType_t stackSize;
} Task_t;

struct Task {
  TaskHandle_t handle;
  void *data;
  UBaseType_t priority;
  UBaseType_t stack;
};

typedef void(TaskFunc_t)(void *);

} // namespace ruth
