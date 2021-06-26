/*
    Ruth
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

#ifndef _ruth_pwm_cmd_hpp
#define _ruth_pwm_cmd_hpp

#include "ArduinoJson.h"
#include "misc/ruth_task.hpp"

#include "dev_pwm/hardware.hpp"

namespace pwm {

class Command {
public:
  Command(Hardware *hardware, const JsonObject &obj);
  virtual ~Command();

  Command() = delete;                 // no default cmds
  Command(const Command &s) = delete; // no copies

  // member access
  const char *name() { return _name; }

  // public API for task info and control
  void kill();
  void notify() const { xTaskNotify(_task.handle, 0, eIncrement); }
  void run();
  bool running() const { return (_task.handle == nullptr) ? false : true; }

protected:
  void fadeTo(uint32_t duty);
  uint32_t getDuty() { return _hw->duty(); };
  inline Hardware *hardware() { return _hw; }
  inline bool keepRunning() const { return _run; }
  inline void loopData(Command *obj) { _task.data = obj; }
  void loopFunction(ruth::TaskFunc_t *func) { _loop_func = func; }
  uint32_t notifyValue() const { return _notify_val; }
  void pause(uint32_t);
  xTaskHandle taskHandle() { return _task.handle; }
  const char *taskName(xTaskHandle handle = nullptr) { return pcTaskGetTaskName(handle); }

protected:
  Hardware *_hw;

private:
  char _name[32] = {};           // name of this cmd
  xTaskHandle _parent = nullptr; // task handle of parent for notification purposes

  // Task Specific member variables
  uint32_t _notify_val = 0;

  ruth::TaskFunc_t *_loop_func = nullptr; // pointer to the loop func for the task
  Command *_loop_data = nullptr;          // pointer to pass to the loop func

  bool _run = true;

  ruth::Task _task = {};

private:
  static void runTask(void *task_instance);
};

typedef std::unique_ptr<Command> CmdWrapped;
} // namespace pwm

#endif
