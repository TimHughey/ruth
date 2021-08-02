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

#include <cstdint>
#include <memory.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace firmware {

class OTA {
public:
  typedef enum : uint32_t { START = 0xb001, CANCEL, FINISH } Notifies;

public:
  OTA(TaskHandle_t notify_task, const char *file, const char *ca_start);
  ~OTA();

  static void captureBaseUrl(const char *url);

  void start();

  static void handlePendingIfNeeded(const uint32_t valid_ms);

private:
  void core(); // main task loop
  static void coreTask(void *task_data);

  bool perform();

  void taskNotify(Notifies val);

private:
  static constexpr size_t _url_max_len = 256;
  char _url[_url_max_len];

  TaskHandle_t _notify_task;
  const char *_ca_start;
  uint64_t _start_at;
  uint32_t _elapsed_ms;

  bool _run_task = true;
};

} // namespace firmware

#endif
