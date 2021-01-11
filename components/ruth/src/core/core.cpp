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

using std::max;
using std::min;

Core::Core() {
  firstHeap_ = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  availHeap_ = heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

void Core::_loop() {
  TickType_t notify_ticks = 0;

  if (cli_->running()) {
    notify_ticks = Profile::coreLoopTicks();
  } else {
    uint8_t key_buff[2] = {};
    auto bytes = uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, key_buff, 1,
                                 Profile::coreLoopTicks());

    if (bytes > 0) {
      switch (key_buff[0]) {
      case 'c':
        cli_->start();
        break;
      case ' ':
      case '\r':
        ESP_LOGI("Core", "hint: press \'c'\' for command line interface");
        break;
      }
    }
  }

  // BaseType_t xTaskNotifyWait(
  //     uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit,
  //     uint32_t * pulNotificationValue, TickType_t xTicksToWait);

  uint32_t val = 0;
  auto notify_rc = xTaskNotifyWait(0x00, ULONG_MAX, &val, notify_ticks);

  if (notify_rc == pdTRUE) {
    const NotifyVal_t notify_val = static_cast<NotifyVal_t>(val);

    switch (notify_val) {
    case NotifyTrackHeap:
      trackHeap();
      break;

    default:
      break;
    }
  }
}

void Core::_start(xTaskHandle app_task) {
  app_task_ = app_task;

  cli_ = new CLI();
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
  trackHeap();
  bootComplete();
  OTA::handlePendPartIfNeeded();

  if (Binder::cliEnabled()) {
    cli_->start();
  }

  if (Profile::watchStacks()) {
    watcher_ = new Watcher();
    watcher_->start();
  }
}

void Core::bootComplete() {
  // boot up is successful, process any previously committed NVS messages
  // and record the successful boot

  num_tasks_ = uxTaskGetNumberOfTasks();

  // lower main task priority since it's really just a watcher now
  UBaseType_t priority = uxTaskPriorityGet(nullptr);

  if (priority > priority_) {
    vTaskPrioritySet(nullptr, priority_);
  }

  report_timer_ = xTimerCreate("core_report", pdMS_TO_TICKS(heap_track_ms_),
                               pdTRUE, nullptr, &reportTimer);
  vTimerSetTimerID(report_timer_, this);
  xTimerStart(report_timer_, pdMS_TO_TICKS(0));

  UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(nullptr);

  auto stack_used =
      100.0 - ((float)stack_high_water / (float)stack_size_ * 100.0);
  ESP_LOGI(TAG, "BOOT COMPLETE in %0.2fs tasks[%d] stack used[%0.1f%%] hw[%u]",
           (float)core_elapsed_, num_tasks_, stack_used, stack_high_water);
}

void Core::consoleTimestamp() {
  if (Binder::cliEnabled()) {
    return;
  }

  if (timestamp_first_report_ == true) {
    // leave first report set to true, it will be set to false later
    timestamp_first_report_ = false;
  } else {

    if (timestamp_elapsed_ < Profile::timestampMS()) {
      return;
    }
  }

  // grab the timestamp frequency seconds from the Profile and cache locally
  if (timestamp_freq_ms_ == 0.0) {
    timestamp_freq_ms_ = Profile::timestampMS();
  }

  UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(nullptr);
  auto stack_used =
      100.0 - ((float)stack_high_water / (float)stack_size_) * 100.0;

  ESP_LOGI(TAG, ">> %s << %s stack used[%0.1f%%] hw[%u]", DateTime().c_str(),
           Net::hostname(), stack_used, stack_high_water);

  timestamp_elapsed_.reset();
}

void Core::startEngines() {
  if (engines_started_) {
    return;
  }

  // NOTE:
  //  startIfEnabled() checks if the engine is enabled in the Profile
  //   1. if so, creates the singleton and starts the engine
  //   2. if not, allocates nothing and does not start the engine
  DallasSemi::startIfEnabled();
  I2c::startIfEnabled();
  PulseWidth::startIfEnabled();

  engines_started_ = true;
}

void Core::trackHeap() {
  uint32_t max_alloc = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  if (max_alloc < (5 * 1024)) {
    Restart("heap fragmentation", __PRETTY_FUNCTION__);
  }

  if (Net::waitForReady(0) == true) {
    Remote reading;

    reading.publish();
  }
}

// STATIC!
Core_t *Core::_instance_() { return &__singleton__; }

} // namespace ruth
