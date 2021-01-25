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

#ifndef ruth_core_hpp
#define ruth_core_hpp

#include <cstdlib>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <sys/time.h>
#include <time.h>

#include "cli/cli.hpp"
#include "core/ota.hpp"
#include "core/watcher.hpp"
#include "lightdesk/control.hpp"
#include "local/types.hpp"
#include "misc/datetime.hpp"
#include "misc/elapsed.hpp"
#include "misc/restart.hpp"

namespace ruth {
using std::unique_ptr;
using namespace ruth;

typedef class Core Core_t;

class Core {
public:
  Core(); // SINGLETON
  static void start(TaskHandle_t app_task) { i()->_start(app_task); };
  static void loop() { i()->_loop(); };
  static bool queuePayload(MsgPayload_t_ptr payload_ptr) {
    return i()->_queuePayload(move(payload_ptr));
  };
  static void reportTimer(TimerHandle_t handle) {
    Core_t *core = (Core_t *)pvTimerGetTimerID(handle);

    if (core) {
      core->trackHeap();
    } else {
      using TR = reading::Text;
      TR::rlog("%s core==nullptr", __PRETTY_FUNCTION__);
    }
  }

  static LightDeskControl_t *lightDeskControl() { return i()->_lightdesk_ctrl; }

  static bool enginesStarted() { return i()->_engines_started; };
  static TaskHandle_t taskHandle() { return i()->_app_task; }

private:
  // private methods for singleton
  static Core_t *i();
  void _loop();
  bool _queuePayload(MsgPayload_t_ptr payload_ptr);
  void _start(xTaskHandle app_task);

  // private functions for class
  void bootComplete();

  void startEngines();
  void trackHeap();

private:
  TaskHandle_t _app_task;
  UBaseType_t _priority = 1;
  elapsedMillis _core_elapsed;

  size_t _stack_size = CONFIG_ESP_MAIN_TASK_STACK_SIZE;

  // heap monitoring
  uint32_t _heap_track_ms = 5 * 1000;
  size_t _heap_first = 0;
  size_t _heap_avail = 0;

  // console timestamp reporting
  bool _timestamp_first_report = true;
  uint64_t _timestamp_freq_ms = 60 * 1000;
  elapsedMillis _timestamp_elapsed;

  // cmd queue
  const int _cmd_queue_max_depth = 6;
  QueueHandle_t _cmd_q = nullptr;

  // task tracking
  UBaseType_t _task_count;
  bool _engines_started = false;

  // remote reading reporting timer
  TimerHandle_t _report_timer = nullptr;

  // Task Stack Watcher
  Watcher_t *_watcher = nullptr;

  // Light Desk Controller
  LightDeskControl_t *_lightdesk_ctrl = nullptr;

  // Command Line Interface
  CLI_t *_cli = nullptr;

  // OTA task, when needed
  OTA_t *_ota = nullptr;
};

} // namespace ruth

#endif
