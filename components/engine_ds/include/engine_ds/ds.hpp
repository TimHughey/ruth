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

#include "dev_ds/ds.hpp"
#include "message/handler.hpp"
#include "message/in.hpp"

#include <cstdlib>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ruth {
namespace ds {

class Engine : public message::Handler {
public:
  struct Opts {
    const char *unique_id;

    struct {
      uint32_t stack = 4096;
      uint32_t priority = 1;
    } command;

    struct {
      uint32_t stack = 4096;
      uint32_t priority = 1;
      uint32_t send_ms = 7000;
      uint32_t loops_per_discover = 10;
    } report;
  };

  enum Notifies : uint32_t { QUEUED_MSG = 0xa000, CMD_ENDING = 0x9000 };
  enum Tasks : size_t { CORE = 0, REPORT, COMMAND };

private:
  Engine(const Opts &opts);
  ~Engine() = default;

public:
  static void command(void *data); // task loop
  static void report(void *data);  // task loop (reports and discover)
  static void start(const Opts &opts);
  void stop();

  void wantMessage(message::InWrapped &msg) override;

private:
  enum DocKinds : uint32_t { CMD = 1 };

private:
  void discover(const uint32_t loops_per_discover);
  Device *findDevice(const char *ident);

private:
  Opts _opts;
  Device *_known[25] = {};

  TaskHandle_t _tasks[Tasks::COMMAND + 1] = {};

  static constexpr size_t max_devices = sizeof(_known) / sizeof(Device *);
  static constexpr size_t max_queue_depth = 5;
};

} // namespace ds
} // namespace ruth
