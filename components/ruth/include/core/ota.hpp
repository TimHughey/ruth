/*
    ota.hpp - Ruth Core OTA Class
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

#ifndef _ruth_core_ota_hpp
#define _ruth_core_ota_hpp

#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_spi_flash.h>

#include "local/types.hpp"
#include "misc/elapsedMillis.hpp"
#include "protocols/payload.hpp"

namespace ruth {

using std::unique_ptr;

typedef class OTA OTA_t;
class OTA {
public:
  // start a new task to pocess the OTA update
  static bool inProgress() {
    return (_instance_() ? _instance_()->_ota_in_progress : false);
  }
  static void start() { _instance_()->_start_(); }
  static OTA_t *payload(MsgPayload_t_ptr payload_ptr) {
    return _instance_(move(payload_ptr));
  };

  static void markPartitionValid();

  const unique_ptr<char[]> debug();

private:
  OTA(MsgPayload_t_ptr payload_ptr_t);

  // executed within a new task
  void process();

  // when _instance_() is called with a MsgPayload pointer the
  // actual singleton objected is allocated and the payload processed
  //
  // when _instance_() called without a payload only the existing SINGLETON
  // instance is returned
  static OTA_t *_instance_(MsgPayload_t_ptr payload_ptr = nullptr);

  void _start_(void *task_data = nullptr) {
    if (_ota_in_progress) {
      ESP_LOGW("OTATask", "OTA in progress, ignoring request");
      return;
    }

    _ota_in_progress = true;

    // this (object) is passed as the data to the task creation and is
    // used by the static runEngine method to call the run method
    ::xTaskCreate(&runTask, "OTATask", _task.stackSize, this, _task.priority,
                  &_task.handle);
  }

  // Task implementation
  static void runTask(void *task_instance) {
    OTA_t *task = (OTA_t *)task_instance;
    task->process();
  }

  static esp_err_t httpEventHandler(esp_http_client_event_t *evt);

private:
  Task_t _task = {.handle = nullptr,
                  .data = nullptr,
                  .lastWake = 0,
                  .priority = 10,
                  .stackSize = (5 * 1024)};

  bool _ota_in_progress = false;

  string_t _uri;
};

} // namespace ruth

#endif
