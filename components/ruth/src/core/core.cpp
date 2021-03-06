/*
    core.cpp - Ruth Core (C++ version app_main())
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

#include <driver/uart.h>

#include "core/binder.hpp"
#include "core/core.hpp"
#include "devs/base/base.hpp"
#include "engines/ds.hpp"
#include "engines/i2c.hpp"
#include "engines/pwm.hpp"
#include "misc/status_led.hpp"
#include "net/network.hpp"
#include "protocols/mqtt.hpp"

namespace ruth {

static const char *TAG = "Core";

static Core_t __singleton__;

Core::Core() {
  _heap_first = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  _heap_avail = heap_caps_get_free_size(MALLOC_CAP_8BIT);

  _cmd_q = xQueueCreate(_cmd_queue_max_depth, sizeof(MsgPayload_t *));
}

void Core::_loop() {
  using TR = reading::Text;

  TickType_t wait_ticks = Profile::coreLoopTicks();

  if (_cli->running() == false) {
    // when the CLI is not running provide a way to start it up

    uint8_t key_buff[15] = {0x00}; // sized to allow for spurious chars
    constexpr size_t max = sizeof(key_buff) - 1;
    auto bytes = uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, key_buff, max, 0);

    for (auto k = 0; ((bytes > 0) && (k < max)); k++) {
      const uint8_t key = key_buff[k];

      if (key == 'c') {
        _cli->start();
        break;
      } else if ((key == ' ') || (key == '\r')) {
        ESP_LOGI("Core", "hint: press \'c'\' for command line interface");
        break;
      }
    }
  }

  MsgPayload_t *payload = nullptr;
  auto queue_rc = xQueueReceive(_cmd_q, &payload, wait_ticks);

  // if we received a queued message proceess it
  // otherwise ignore the failure because it was most likely a timeout
  if (queue_rc == pdTRUE) {
    // wrap in a unique_ptr so it is freed when out of scope
    unique_ptr<MsgPayload_t> payload_ptr(payload);

    if (payload->matchSubtopic("raw")) {
      CLI::remoteLine(payload);
    } else if (payload->matchSubtopic("restart")) {
      Restart("restart requested", __PRETTY_FUNCTION__);
    } else if (payload->matchSubtopic("ota")) {

      if (_lightdesk) {
        _lightdesk->stop();
      }

      if (_ota == nullptr) {
        _ota = new OTA();
        _ota->start();
        _ota->handleCommand(*payload); // handleCommand() logs as required
      }
    } else if (payload->matchSubtopic("ota_cancel")) {
      if (_ota) {
        delete _ota;
        _ota = nullptr;
      }
    } else {
      TR::rlog("[CORE] unknown message subtopic=\"%s\"", payload->subtopic());
    }
  }
}

bool Core::_queuePayload(MsgPayload_t_ptr payload_ptr) {
  auto rc = false;
  auto q_rc = pdTRUE;

  // when the queue is full pop the first queued command to make room
  if (_cmd_q && (uxQueueSpacesAvailable(_cmd_q) == 0)) {
    MsgPayload_t *old = nullptr;

    q_rc = xQueueReceive(_cmd_q, &old, pdMS_TO_TICKS(10));

    if ((q_rc == pdTRUE) && (old != nullptr)) {
      delete old;
    }
  }

  if (q_rc == pdTRUE) {
    MsgPayload_t *payload = payload_ptr.get();
    q_rc = xQueueSendToBack(_cmd_q, (void *)&payload, pdMS_TO_TICKS(10));

    if (q_rc == pdTRUE) {
      // since the payload was queued and we need to keep it until processed
      // release it from the unique ptr
      payload_ptr.release();
      rc = true;
    }
  }
  return rc;
}

void Core::_start(xTaskHandle app_task) {
  _app_task = app_task;

  _cli = new CLI();
  StatusLED::init();
  Binder::init();

  StatusLED::brighter();

  // get the Network up and running as soon as possible
  Net::start();

  MQTT::start();
  StatusLED::brighter();

  // safety net 1:
  //    wait for the name to be set for 90 seconds, if the name is not
  //    set within in 90 seconds then there's some problem so reboot
  if (Net::waitForName(45000) == false) {
    ESP_LOGE(pcTaskGetTaskName(nullptr), "name and profile assignment failed");
    Restart().now();
  }

  Net::waitForNormalOps(portMAX_DELAY);

  // now that we have our name and protocols up, proceed with starting
  // the engines and performing actual work
  startEngines();

  if (Binder::lightDeskEnabled() || Profile::lightDeskEnabled()) {
    _lightdesk = lightdesk::LightDesk::make_shared();
  }

  trackHeap();
  bootComplete();
  OTA::partitionHandlePendingIfNeeded();

  if (Binder::cliEnabled()) {
    _cli->start();
  }

  if (Profile::watchStacks()) {
    _watcher = new Watcher();
    _watcher->start();
  }
}

void Core::bootComplete() {
  // boot up is successful, process any previously committed NVS messages
  // and record the successful boot

  _task_count = uxTaskGetNumberOfTasks();

  // lower main task priority since it's really just a watcher now
  UBaseType_t priority = uxTaskPriorityGet(nullptr);

  if (priority > _priority) {
    vTaskPrioritySet(nullptr, _priority);
  }

  _report_timer = xTimerCreate("core_report", pdMS_TO_TICKS(_heap_track_ms),
                               pdTRUE, nullptr, &reportTimer);
  vTimerSetTimerID(_report_timer, this);
  xTimerStart(_report_timer, pdMS_TO_TICKS(0));

  UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(nullptr);

  auto stack_used =
      100.0 - ((float)stack_high_water / (float)_stack_size * 100.0);
  ESP_LOGI(TAG, "BOOT COMPLETE in %0.2fs tasks[%d] stack used[%0.1f%%] hw[%u]",
           (float)_core_elapsed, _task_count, stack_used, stack_high_water);
}

void Core::startEngines() {
  // if the engines are already started obviously don't start them again.

  // secondly, if this host hasn't yet been assigned a name (it's new) then
  // don't engines.  by not starting the engines we prevent sending device
  // readings which in turn will prevent device creation centrally.

  // in other words, we only want to create devices centrally once this host
  // has been assigned a name.
  if (_engines_started || Net::hostIdAndNameAreEqual()) {
    return;
  }

  // NOTE:
  //  startIfEnabled() checks if the engine is enabled in the Profile
  //   1. if so, creates the singleton and starts the engine
  //   2. if not, allocates nothing and does not start the engine
  DallasSemi::startIfEnabled();
  I2c::startIfEnabled();
  PulseWidth::startIfEnabled();

  _engines_started = true;
}

void Core::trackHeap() {
  uint32_t max_alloc = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  if (max_alloc < (5 * 1024)) {
    Restart("heap fragmentation", __PRETTY_FUNCTION__);
  }

  if (Net::waitForNormalOps(0) == true) {
    Remote reading;

    reading.publish();
  }
}

// STATIC!
Core_t *Core::i() { return &__singleton__; }

} // namespace ruth
