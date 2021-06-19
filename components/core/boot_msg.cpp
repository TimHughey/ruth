/*
  Ruth
  (C)opyright 2021  Tim Hughey

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

#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "boot_msg.hpp"

namespace message {

static const char *TAG = "Core";

Boot::Boot(const size_t stack_size, const uint32_t elapsed_ms)
    : _stack_size(stack_size), _elapsed_ms(elapsed_ms) {
  _filter.addLevel("host");
  _filter.addLevel("boot");
}

void Boot::assembleData(JsonObject &data) {
  auto task_count = uxTaskGetNumberOfTasks();
  UBaseType_t high_water = uxTaskGetStackHighWaterMark(nullptr);

  ESP_LOGI(TAG, "BOOT COMPLETE in %ums tasks[%d] stack[%u] hw[%u]", _elapsed_ms, task_count, _stack_size,
           high_water);

  data["elapsed_ms"] = _elapsed_ms;
  data["tasks"] = task_count;

  JsonObject stack = data.createNestedObject("stack");
  stack["size"] = _stack_size;
  stack["highwater"] = high_water;
}

} // namespace message
