/*
  Ruth
  (C)opyright 2020  Tim Hughey

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
#include <memory>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include <sys/time.h>
#include <time.h>

#include "message/handler.hpp"

namespace ruth {

class Core : public message::Handler {
public:
  Core();                                // SINGLETON
  Core(const Core &) = delete;           // prevent copies
  void operator=(const Core &) = delete; // prevent assignments

  static void boot();
  static bool enginesStarted();
  static void loop();

  static void reportTimer(TimerHandle_t handle);

  void wantMessage(message::InWrapped &msg) override;

private:
  // private functions for class
  void bootComplete();

  void sntp();
  void startEngines();
  void startMqtt();
  void trackHeap();

private:
  enum DocKinds : uint32_t { PROFILE = 1, RESTART, OTA, BINDER };

private:
  UBaseType_t _priority = 1;
  uint64_t _core_start_at;

  size_t _stack_size = CONFIG_ESP_MAIN_TASK_STACK_SIZE;

  // heap monitoring
  uint32_t _heap_track_ms = 5 * 1000;
  size_t _heap_first = 0;
  size_t _heap_avail = 0;

  // cmd queue
  static constexpr int _max_queue_depth = 6;

  // task tracking
  bool _engines_started = false;

  // host report timer
  TimerHandle_t _report_timer = nullptr;

  // Task Stack Watcher
  // Watcher_t *_watcher = nullptr;
};

} // namespace ruth

#endif
