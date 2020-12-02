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
#include "misc/elapsed.hpp"
#include "protocols/payload.hpp"

namespace ruth {

typedef class OTA OTA_t;

class OTA {
public:
  OTA(){}; // SINGLETON
  // start a new task to pocess the OTA update
  static void handlePendPartIfNeeded() {
    _instance_()->_handlePendPartIfNeeded_();
  }
  static bool inProgress() { return _instance_()->_ota_in_progress; }
  static bool queuePayload(MsgPayload_t_ptr payload_ptr);
  static bool queuePayload(const char *payload);
  static void start() { _instance_()->_start_(); }

  static void markPartitionValid(TimerHandle_t handle);

private:
  OTA(MsgPayload_t_ptr payload_ptr_t);

  void _handlePendPartIfNeeded_();
  static OTA_t *_instance_();

  // executed within a new task
  void process();

  void _start_(void *task_data = nullptr) {
    // ignore requests for OTA if one is in progress
    if (_ota_in_progress) {
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

  static TaskHandle_t taskHandle() { return _instance_()->_task.handle; }
  static TimerHandle_t timerHandle() { return _instance_()->_valid_timer; }

  static esp_err_t httpEventHandler(esp_http_client_event_t *evt);

private:
  Task_t _task = {.handle = nullptr,
                  .data = nullptr,
                  .priority = 1, // allow reporting to continue
                  .stackSize = (5 * 1024)};

  TimerHandle_t _valid_timer = nullptr;
  MsgPackPayload_t *_payload = nullptr;

  bool _ota_in_progress = false;
  bool _ota_marked_valid = false;
  uint32_t _ota_valid_ms = 60 * 1000;

  OtaUri_t _uri;
};

} // namespace ruth

#endif
