//  Ruth
//  Copyright (C) 2020  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "misc/elapsed.hpp"
#include "ru_base/types.hpp"

#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <memory.h>
#include <optional>
#include <string>
#include <string_view>

namespace ruth {

namespace firmware {

typedef const std::string_view csv;

class OTA {
public:
  typedef enum : uint32_t { START = 0xb001, CANCEL, FINISH, ERROR } Notifies;

public:
  OTA(const char *base_url, const char *file, const char *ca_start) noexcept;
  ~OTA() noexcept;

  static void handlePendingIfNeeded(const uint32_t valid_ms);

private:
  Notifies core() noexcept; // main task loop
  static void coreTask(void *task_data);

  void notifyParent(Notifies notify_val);

private:
  // order dependent for object initialization
  TaskHandle_t notify_task;
  const char *ca_start;
  string url;
  TaskHandle_t self_task;

  // order independent
  std::optional<void *> ota_handle;
  Elapsed e;

public:
  static constexpr const char *TAG{"ota"};
  static constexpr size_t URL_MAX_LEN{512};
};

} // namespace firmware
} // namespace ruth
