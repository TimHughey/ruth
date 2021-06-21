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

#ifndef _ruth_pwm_engine_hpp
#define _ruth_pwm_engine_hpp

#include <cstdlib>

#include "dev_pwm/pwm.hpp"
#include "message/handler.hpp"
#include "message/in.hpp"
#include "misc/ruth_task.hpp"

namespace pwm {

class Engine : public message::Handler {
public:
  struct Opts {
    struct command {
      UBaseType_t stack = 4096;
      UBaseType_t priority = 13;
    };

    struct report {
      UBaseType_t stack = 3048;
      UBaseType_t priority = 1;
      uint32_t send_ms = 7000;
    };
  };

public:
  typedef device::PulseWidth Device;

private:
  Engine();
  ~Engine() = default;

public:
  void command(void *data);
  void report(void *data);

  void start(Opts &opts);
  void stop();

  void wantMessage(message::InWrapped &msg) override;

private:
  ruth::Task_t _task;

  Device _known[4];
  static constexpr size_t _num_devices = sizeof(_known) / sizeof(Device);
};
} // namespace pwm

#endif // pwm_engine_hpp
