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
#include <memory>
#include <string>

#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <sys/time.h>
#include <time.h>

#include "core/ota.hpp"
#include "misc/elapsedMillis.hpp"
#include "misc/local_types.hpp"
#include "misc/restart.hpp"

namespace ruth {
using std::unique_ptr;

typedef class Core Core_t;

class Core {
public:
  static void start(TaskHandle_t app_task) { _instance_()->_start(app_task); };
  static void loop() { _instance_()->_loop(); };

  static uint32_t batteryMilliVolt() { return _instance_()->_battMV(); };
  static uint32_t vref() { return 1100; };

  static unique_ptr<char[]> dateTimeString(time_t t = 0);
  static void otaRequest(OTA_t *ota) { _instance_()->_otaRequest(ota); };
  static void restartRequest() { _instance_()->_restartRequest(); };

private:
  // constructor is private, this is a singleton
  Core();
  // private methods for singleton
  static Core_t *_instance_();
  void _loop();
  void _start(xTaskHandle app_task);
  uint32_t _battMV();
  void _otaRequest(OTA_t *ota) {
    // store the pointer to the ota request
    ota_request_ = ota;

    // notify app_main task to process it via Core::_loop()
    xTaskNotify(app_task_, 0x0, eIncrement);
  };

  void _restartRequest() {
    // flag a restart was requested
    restart_request_ = true;

    // notify app_main task to process it via Core::_loop()
    xTaskNotify(app_task_, 0x0, eIncrement);
  };

  // private functions for class
  void bootComplete();
  void consoleTimestamp();
  void markOtaValid();

  // handle any Core requests
  void handleRequests() {

    // if, somehow, an OTA update an a researt were both requested
    // favor the OTA request
    if (ota_request_) {
      ota_request_ = nullptr;
      ota_request_->start();
    }

    if (restart_request_) {
      Restart::restart("restart requested", __PRETTY_FUNCTION__);
    }
  }

  void startEngines();
  void trackHeap();

private:
  TaskHandle_t app_task_;
  UBaseType_t priority_ = 1;
  elapsedMillis core_elapsed_;
  bool boot_complete_ = false;
  bool engines_started_ = false;

  // heap monitoring
  bool heap_track_first_ = true;
  float heap_track_secs_ = 3.0;
  elapsedMillis heap_track_elapsed_;
  size_t firstHeap_ = 0;
  size_t availHeap_ = 0;
  size_t minHeap_ = UINT32_MAX;
  size_t maxHeap_ = 0;
  uint32_t batt_mv_ = 0;

  // console timestamp reporting
  bool timestamp_first_report_ = true;
  float timestamp_freq_secs_ = 0.0;
  elapsedMillis timestamp_elapsed_;

  // task tracking
  UBaseType_t num_tasks_;

  // OTA and Restart
  OTA_t *ota_request_ = nullptr;
  bool restart_request_ = false;
  bool ota_marked_valid_ = false;
  float ota_valid_secs_ = 60.0;

  // battery voltage
  esp_adc_cal_characteristics_t *adc_chars_ = nullptr;
  esp_adc_cal_value_t adc_cal_;
  uint32_t batt_measurements_ = 64; // measurements to avg out noise

  static const adc_channel_t battery_adc_ = ADC_CHANNEL_7;
};

} // namespace ruth

// class TimestampTask {
//
// public:
//   void watchTaskStacks();
//
// private:
//   TickType_t _last_wake;
//   const TickType_t _loop_frequency = pdMS_TO_TICKS(3000);
//   time_t _timestamp_freq_secs = (12 * 60 * 60); // twelve (12) hours
//
//   typedef struct {
//     xTaskHandle handle;
//     uint32_t stack_high_water;
//     uint32_t stack_depth = 0;
//     bool task = false;
//   } TaskStat_t;
//
//   typedef TaskStat_t *TaskStat_ptr_t;
//
//   typedef std::pair<string_t, TaskStat_ptr_t> TaskMapItem_t;
//
//   // key(task name) entry(task stack high water)
//   unordered_map<string_t, TaskStat_ptr_t> _task_map;
//   bool _tasks_ongoing_report = false;
//
//   void reportTaskStacks();
//   void updateTaskData();
// };
#endif
