/*
    Ruth
    Copyright (C) 2017  Tim Hughey

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

#pragma once

#include "dev_pwm/pwm.hpp"
#include "message/handler.hpp"
#include "message/in.hpp"

#include <cstdlib>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ruth {

namespace pwm {

class Engine : public message::Handler {
public:
  struct Opts {
    const char *unique_id;

    struct {
      uint32_t stack = 4096;
      uint32_t priority = 13;
    } command;

    struct {
      uint32_t stack = 3048;
      uint32_t priority = 1;
      uint32_t send_ms = 7000;
    } report;
  };

  enum Notifies : uint32_t { QUEUED_MSG = 0xa000, CMD_ENDING = 0x9000 };

  typedef PulseWidth Device;

private:
  Engine(const char *unique_id, uint32_t report_send_ms);
  ~Engine() = default;

public:
  static void command(void *data);
  static void report(void *data);

  static void start(Opts &opts);
  void stop();

  void wantMessage(message::InWrapped &msg) override;

private:
  enum DocKinds : uint32_t { CMD = 1 };

private:
  Device _known[4];
  static char _ident[32];

  TaskHandle_t _report_task = nullptr;
  uint32_t _report_send_ms = 13000;
  TaskHandle_t _command_task = nullptr;

  static constexpr size_t _num_devices = sizeof(_known) / sizeof(Device);
  static constexpr size_t _max_queue_depth = 5;
};

} // namespace pwm
} // namespace ruth
