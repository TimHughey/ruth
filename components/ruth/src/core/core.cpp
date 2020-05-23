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

#include "core/core.hpp"
#include "devs/base.hpp"
#include "devs/pwm_dev.hpp"
#include "engines/ds.hpp"
#include "engines/i2c.hpp"
#include "engines/pwm.hpp"
#include "misc/nvs.hpp"
#include "misc/status_led.hpp"
#include "net/network.hpp"
#include "protocols/mqtt.hpp"

namespace ruth {

static const char *TAG = "Core";

static Core_t *__singleton__ = nullptr;

using std::max;
using std::min;

Core::Core() {

  firstHeap_ = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  availHeap_ = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  // Characterize and setup ADC for measuring battery millivolts
  adc_chars_ = (esp_adc_cal_characteristics_t *)calloc(
      1, sizeof(esp_adc_cal_characteristics_t));
  adc_cal_ = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11,
                                      ADC_WIDTH_BIT_12, vref(), adc_chars_);

  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten((adc1_channel_t)battery_adc_, ADC_ATTEN_DB_11);
}

void Core::_loop() {
  //
  // NOTE:
  //  unless doing actual work on the first call or subsequent calls
  //   (e.g. trackHeap(), consoleTimestamp()) all functions are designed
  //   to execute very quickly
  //

  // safety net 1:
  //    wait for the name to be set for 90 seconds, if the name is not
  //    set within in 90 seconds then there's some problem so reboot
  if (Net::waitForName(90000) == false) {
    Restart::now();
  }

  // safety net 2:
  //    wait for the transport to be ready for up to 60 seconds (60000ms).
  //    if the overall ready bit is not set then an issue after startup
  //    (after startup since we wouldn't be here if transport never
  //    became available)
  if (Net::waitForReady(60000) == false) {
    Restart::now();
  }

  // now that we have our name and protocols up, proceed with starting
  // the engines and performing actual work
  startEngines();

  trackHeap();
  bootComplete();
  consoleTimestamp();
  markOtaValid();
  handleRequests();

  // TODO
  // implement watchTaskStacks()

  // sleep for one (1) second or until notified
  ulTaskNotifyTake(pdTRUE, Profile::coreLoopTicks());
}

void Core::_start(xTaskHandle app_task) {
  app_task_ = app_task;

  // initialize the StatusLED singleton
  StatusLED::init();

  // get NVS started, it is needed by Net
  NVS::init();
  StatusLED::brighter();

  // get the Network up and running as soon as possible
  Net::start();

  MQTT::start();
  StatusLED::brighter();
}

uint32_t Core::_battMV() {
  uint32_t batt_raw = 0;
  uint32_t batt_mv = 0;

  // per ESP32 documentation ADC readings typically include noise.
  // so, perform more than one reading then take the average
  for (uint32_t i = 0; i < batt_measurements_; i++) {
    batt_raw += adc1_get_raw((adc1_channel_t)battery_adc_);
  }

  batt_raw /= batt_measurements_;

  // the pin used to measure battery millivolts is connected to a voltage
  // divider so double the voltage
  batt_mv = esp_adc_cal_raw_to_voltage(batt_raw, adc_chars_) * 2;

  return batt_mv;
}

void Core::bootComplete() {
  if (boot_complete_)
    return;

  // boot up is successful, process any previously committed NVS messages
  // and record the successful boot

  num_tasks_ = uxTaskGetNumberOfTasks();

  NVS::processCommittedMsgs();
  NVS::commitMsg("BOOT", "LAST SUCCESSFUL BOOT");

  // lower main task priority since it's really just a watcher now
  UBaseType_t priority = uxTaskPriorityGet(NULL);

  if (priority > priority_) {
    vTaskPrioritySet(NULL, priority_);

    ESP_LOGI(TAG, "app_main() priority reduced %d -> %d", priority, priority_);
  }

  UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(nullptr);

  ESP_LOGI(TAG, "boot complete elapsed=%0.2fs stack_hw=%d num_task=%d",
           core_elapsed_.asSeconds(), stack_high_water, num_tasks_);

  boot_complete_ = true;
}

void Core::consoleTimestamp() {
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

  int delta;
  size_t curr_heap, max_alloc = 0;

  curr_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  delta = availHeap_ - curr_heap;
  availHeap_ = curr_heap;

  minHeap_ = min(curr_heap, minHeap_);
  maxHeap_ = max(curr_heap, maxHeap_);

  max_alloc = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  timestamp_elapsed_.reset();

  const char *name = Net::getName().c_str();
  char delta_str[13] = {};

  if (delta < 0) {
    snprintf(delta_str, sizeof(delta_str), "(%05d)", delta * -1);
  } else {
    snprintf(delta_str, sizeof(delta_str), "%05d", delta);
  }

  ESP_LOGI(name, "%s hc=%uk hf=%uk hl=%uk d=%s ma=%uk batt=%0.2fv",
           dateTimeString().get(), (curr_heap / 1024), (firstHeap_ / 1024),
           (maxHeap_ / 1024), delta_str, (max_alloc / 1024),
           (float)(batt_mv_ / 1024.0));

  if (timestamp_first_report_ && (timestamp_freq_ms_ > (5 * 60 * 1300.0))) {
    timestamp_first_report_ = false;
    ESP_LOGI(name, "--> next timestamp report in %0.2f minutes",
             (float)(timestamp_freq_ms_ / (60.0 * 1000.0)));
  }

  timestamp_elapsed_.reset();
}

void Core::markOtaValid() {
  if (ota_marked_valid_)
    return;

  if (core_elapsed_.asSeconds() > ota_valid_secs_) {
    // safety net 3:
    //    only after the core has been up for more than the configured
    //    seconds mark the OTA partition as valid
    OTA::markPartitionValid();

    ota_marked_valid_ = true;
  }
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
  if (heap_track_first_) {
    heap_track_first_ = false;
  } else if (heap_track_elapsed_.asSeconds() < heap_track_secs_) {
    return;
  } else {
    heap_track_elapsed_.reset();
  }

  batt_mv_ = batteryMilliVolt();

  uint32_t max_alloc = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  if (max_alloc < (5 * 1024)) {
    Restart::restart("max allocate < 5k (heap fragmentation)",
                     __PRETTY_FUNCTION__, 0);
  }

  if (Net::waitForReady(0) == true) {
    remoteReading_ptr_t remote(new remoteReading(batt_mv_));
    remote->publish();
  }

  heap_track_elapsed_.reset();
}

// STATIC!
Core_t *Core::_instance_() {
  if (__singleton__ == nullptr) {
    __singleton__ = new Core();
  }

  return __singleton__;
}

// NOTE:  Use .get() to access the underlying char array
unique_ptr<char[]> Core::dateTimeString(time_t t) {

  const auto buf_size = 28;
  unique_ptr<char[]> buf(new char[buf_size + 1]);

  time_t now = time(nullptr);
  struct tm timeinfo = {};

  localtime_r(&now, &timeinfo);
  strftime(buf.get(), buf_size, "%c", &timeinfo);
  // strftime(buf.get(), buf_size, "%b-%d %R", &timeinfo);

  return move(buf);
}
} // namespace ruth

// void TimestampTask::reportTaskStacks() {
//   if (_task_map.size() == 0)
//     return;
//
//   textReading *rlog = new textReading();
//   textReading_ptr_t rlog_ptr(rlog);
//
//   for_each(_task_map.begin(), _task_map.end(), [rlog](TaskMapItem_t item) {
//     string_t name = item.first;
//     TaskStat_ptr_t stat = item.second;
//
//     if (stat->stack_high_water > 0) {
//       rlog->printf("%s=%d,", name.c_str(), stat->stack_high_water);
//     }
//   });
//
//   rlog->consoleInfo(tTAG);
//   rlog->publish();
//
//   updateTaskData();
// }
//
// void TimestampTask::updateTaskData() {
//   for_each(_task_map.begin(), _task_map.end(), [this](TaskMapItem_t item) {
//     auto stat = item.second;
//
//     stat->stack_high_water = uxTaskGetStackHighWaterMark(stat->handle);
//   });
// }
//
// void TimestampTask::watchTaskStacks() {
//   uint32_t num_tasks = uxTaskGetNumberOfTasks();
//   _task_map.reserve(num_tasks);
//
//   TaskStatus_t *buff = new TaskStatus_t[num_tasks];
//   unique_ptr<TaskStatus_t> buff_ptr(buff);
//   uint32_t run_time;
//
//   uxTaskGetSystemState(buff, num_tasks, &run_time);
//
//   for (uint32_t i = 0; i < num_tasks; i++) {
//     TaskStatus_t task = buff[i];
//
//     if (task.pcTaskName != nullptr) {
//       string_t name = task.pcTaskName;
//
//       TaskStat_ptr_t stat = new TaskStat_t;
//
//       stat->handle = task.xHandle;
//       stat->stack_high_water = task.usStackHighWaterMark;
//
//       _task_map[name] = stat;
//     }
//   }
