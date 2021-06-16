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

// #include "cli/cli.hpp"
// #include "core/ota.hpp"
// #include "watcher.hpp"
// #include "lightdesk/lightdesk.hpp"
// #include "local/types.hpp"
#include "datetime.hpp"
#include "elapsed.hpp"
#include "handler.hpp"
// #include "misc/restart.hpp"

namespace ruth {
using std::unique_ptr;

typedef class Core Core_t;

class Core : public message::Handler {
public:
  Core();                                // SINGLETON
  Core(const Core &) = delete;           // prevent copies
  void operator=(const Core &) = delete; // prevent assignments

  static bool enginesStarted();
  static void loop();

  static void reportTimer(TimerHandle_t handle) {
    Core_t *core = (Core_t *)pvTimerGetTimerID(handle);

    if (core) {
      core->trackHeap();
    } else {
      // using TR = reading::Text;
      // TR::rlog("%s core==nullptr", __PRETTY_FUNCTION__);
    }
  }

  static void start(TaskHandle_t app_task);
  static TaskHandle_t taskHandle();
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
  enum Notifies : uint32_t { SNTP_COMPLETE = 0xa000, WIFI_READY };

private:
  TaskHandle_t _app_task;
  UBaseType_t _priority = 1;
  elapsedMillis _core_elapsed;

  size_t _stack_size = CONFIG_ESP_MAIN_TASK_STACK_SIZE;

  // heap monitoring
  uint32_t _heap_track_ms = 5 * 1000;
  size_t _heap_first = 0;
  size_t _heap_avail = 0;

  // cmd queue
  static constexpr int _max_queue_depth = 6;

  // task tracking
  UBaseType_t _task_count;
  bool _engines_started = false;

  // host report timer
  TimerHandle_t _report_timer = nullptr;

  // Task Stack Watcher
  // Watcher_t *_watcher = nullptr;

  // LightDesk
  // std::shared_ptr<lightdesk::LightDesk_t> _lightdesk = nullptr;

  // Command Line Interface
  // CLI_t *_cli = nullptr;

  // OTA task, when needed
  // OTA_t *_ota = nullptr;
};

} // namespace ruth

#endif
