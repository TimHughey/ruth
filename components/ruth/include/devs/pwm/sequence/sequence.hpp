/*
    include/pwm/sequence/sequence.hpp - Ruth PWM Sequence Class
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

#ifndef _ruth_pwm_sequence_hpp
#define _ruth_pwm_sequence_hpp

#include <memory>
#include <string>
#include <vector>

#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "devs/pwm/sequence/step.hpp"

namespace ruth {
namespace pwm {

using std::vector;

typedef class Sequence Sequence_t;

class Sequence {
public:
  Sequence(const char *pin_desc, const char *name, xTaskHandle parent,
           ledc_channel_config_t *channel, JsonObject &obj);
  ~Sequence();

  Sequence() = delete;                  // no default sequences
  Sequence(const Sequence &s) = delete; // no copies

  // member access
  const string_t &name() { return _name; }
  bool active() const { return _active; }

  // task implementation, control and access
  void run() { _start_(); }
  bool running() const { return (_task.handle == nullptr) ? false : true; }
  void stop();

private:
  string_t _name;        // name of this sequence
  const char *_pin_desc; // pwm pin description
  xTaskHandle _parent;   // task handle of parent for notification purposes
  ledc_channel_config_t *_channel; // ledc channel to control
  bool _repeat = false;            // should sequence repeat or execute once?
  bool _active = false;            // should sequence become active?

  vector<Step_t *> _steps = {}; // the actual steps

  Task_t _task = {.handle = nullptr,
                  .data = nullptr,
                  .lastWake = 0,
                  .priority = 13,
                  .stackSize = 2048};

  union {
    bool _happy;
    const char *ptr;
  };

private:
  void loop(void *data);

  void _start_(void *task_data = nullptr) {

    // already running
    if (_task.handle)
      return;

    // create the task name
    string_t task_name = "pwm-";
    task_name.append(_pin_desc);

    // this (object) is passed as the data to the task creation and is
    // used by the static runEngine method to call the run method
    ::xTaskCreate(&runTask, task_name.c_str(), _task.stackSize, this,
                  _task.priority, &_task.handle);
  }

  // Task implementation
  static void runTask(void *task_instance) {
    Sequence_t *seq = (Sequence_t *)task_instance;
    seq->loop(seq->_task.data);
  }
};
} // namespace pwm
} // namespace ruth

#endif
