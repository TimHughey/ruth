/*
    include/pwm/cmds/cmd.hpp - Ruth PWM Command Class
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

#include <memory>
#include <string>

#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "devs/pwm/cmds/step.hpp"
#include "readings/simple_text.hpp"

namespace ruth {
namespace pwm {

using std::vector;

typedef class Command Command_t;

class Command {
public:
  Command(const char *pin, ledc_channel_config_t *chan, JsonObject &obj);
  virtual ~Command();

  Command() = delete;                 // no default cmds
  Command(const Command &s) = delete; // no copies

  // member access
  const ledc_channel_config_t *channel() const { return _channel; }
  const string_t &name() { return _name; }
  const char *name_cstr() const { return _name.c_str(); }

  void activate(bool yes_or_no) { _active = yes_or_no; }
  bool active() const { return _active; }

  // task implementation, control and access
  void run() { _start_(); }
  bool running() const { return (_task.handle == nullptr) ? false : true; }
  void stop();

protected:
  void loopData(Command_t *obj) { _task.data = obj; }
  void useLoopFunction(TaskFunc_t *func) { _loop_func = func; }

protected:
  TaskFunc_t *_loop_func = nullptr;
  Command_t *_loop_data = nullptr;

private:
  string_t _name;      // name of this cmd
  const char *_pin;    // pwm pin description
  xTaskHandle _parent; // task handle of parent for notification purposes
  ledc_channel_config_t *_channel; // ledc channel to control

  bool _active = false; // should cmd become active?

  Task_t _task = {.handle = nullptr,
                  .data = nullptr,
                  .lastWake = 0,
                  .priority = 13,
                  .stackSize = 2048};

private:
  void _start_(void *task_data = nullptr) {
    // already running or loop function is not set
    if (_task.handle || (_loop_func == nullptr))
      return;

    // create the task name
    string_t task_name = "pwm-";
    task_name.append(_pin);

    // this (object) is passed as the data to the task creation and is
    // used by the static runEngine method to call the run method
    ::xTaskCreate(&runTask, task_name.c_str(), _task.stackSize, _task.data,
                  _task.priority, &_task.handle);
  }

  // Task implementation
  static void runTask(void *task_instance) {
    Command_t *seq = (Command_t *)task_instance;
    seq->_loop_func(seq->_task.data);

    ST::rlog("cmd \"%s\" finished", seq->_name.c_str());

    xTaskNotify(seq->_parent, 0, eIncrement);
    seq->stop();
  }
};
} // namespace pwm
} // namespace ruth

#endif
