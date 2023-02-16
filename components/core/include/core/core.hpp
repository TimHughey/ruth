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

#include "message/handler.hpp"
#include "ota/ota_decls.hpp"

#include <cstdlib>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <memory>
#include <string>
#include <sys/time.h>
#include <time.h>

namespace ruth {

class Core : public message::Handler {

public:
  Core() noexcept;                       // SINGLETON
  Core(const Core &) = delete;           // prevent copies
  void operator=(const Core &) = delete; // prevent assignments

  void loop() noexcept;
  void wantMessage(message::InWrapped &msg) override;

private:
  // private functions for class
  void do_ota(message::InWrapped msg) noexcept;
  static void report_timer(TimerHandle_t handle) noexcept;
  void start_mqtt() noexcept;
  void start_sntp() noexcept;
  void track_heap() noexcept;

private:
  enum DocKinds : uint32_t { PROFILE = 1, RESTART, OTA, BINDER };

private:
  // order dependent
  UBaseType_t priority{1};
  const size_t heap_first;
  size_t heap_avail;
  uint32_t heap_track_ms;
  bool engines_started;
  TimerHandle_t report_timer_handle;
  std::shared_ptr<firmware::OTA> ota; // must use shared_ptr due to incomplete type

  // order independent
  std::string ota_base_url;

  // cmd queue
  static constexpr int MAX_QUEUE_DEPTH{6};
};

} // namespace ruth
