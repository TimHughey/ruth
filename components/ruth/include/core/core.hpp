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
  static void start(TaskHandle_t app_task) { _instance_()->_start(app_task); };
  static void loop() { _instance_()->_loop(); };
  static void reportTimer(TimerHandle_t handle) {
    Core_t *core = (Core_t *)pvTimerGetTimerID(handle);
    core->notifyTrackHeap();
  }

  static bool enginesStarted() { return _instance_()->engines_started_; };
  static uint32_t vref() { return 1100; };

private:
  // private methods for singleton
  static Core_t *_instance_();
  void _loop();
  void _start(xTaskHandle app_task);
  // uint32_t _battMV();

  // private functions for class
  void bootComplete();
  void consoleTimestamp();
  void notifyTrackHeap() {
    xTaskNotify(app_task_, NotifyTrackHeap, eSetValueWithOverwrite);
  }

  void startEngines();
  void trackHeap();

private:
  TaskHandle_t app_task_;
  UBaseType_t priority_ = 1;
  elapsedMillis core_elapsed_;

  size_t stack_size_ = CONFIG_ESP_MAIN_TASK_STACK_SIZE;

  // heap monitoring
  uint32_t heap_track_ms_ = 5 * 1000;
  size_t firstHeap_ = 0;
  size_t availHeap_ = 0;

  // console timestamp reporting
  bool timestamp_first_report_ = true;
  uint64_t timestamp_freq_ms_ = 60 * 1000;
  elapsedMillis timestamp_elapsed_;

  // task tracking
  UBaseType_t num_tasks_;
  bool engines_started_ = false;

  // remote reading reporting timer
  TimerHandle_t report_timer_ = nullptr;

  // Task Stack Watcher
  Watcher_t *watcher_ = nullptr;

  // Command Line Interface
  CLI_t *cli_ = nullptr;
};

} // namespace ruth

#endif
