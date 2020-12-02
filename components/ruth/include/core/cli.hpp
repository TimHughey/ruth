/*
    core/clip.hpp - Ruth Command Line Interface Task
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

#ifndef _ruth_core_cli_hpp
#define _ruth_core_cli_hpp

#include "core/ota.hpp"
#include "local/types.hpp"
#include "misc/datetime.hpp"
#include "net/network.hpp"

namespace ruth {

typedef class CLI CLI_t;

class CLI {
public:
  CLI(){};

  void start() {
    if (_task.handle) {
      return;
    }

    // this (object) is passed as the data to the task creation and is
    // used by the static runEngine method to call the run method
    ::xTaskCreate(&runTask, "Rcli", _task.stackSize, this, _task.priority,
                  &_task.handle);
  }

private:
  void init();
  void loop();

  void registerClearCommand();
  void registerDateCommand();
  void registerOtaCommand();

  // Task implementation
  static void runTask(void *task_instance) {
    CLI_t *cli = (CLI_t *)task_instance;
    cli->init();
    cli->loop();
  }

  static int clearCommand(int argc, char **argv) {
    printf("\033[2J\033[H");
    return 9;
  }

  static int dateCommand(int argc, char **argv) {
    printf("%s\n", DateTime().c_str());
    return 0;
  }

  static int otaCommand(int argc, char **argv);

private:
  Task_t _task = {.handle = nullptr,
                  .data = nullptr,
                  .priority = 1, // allow reporting to continue
                  .stackSize = (5 * 1024)};
};

} // namespace ruth

#endif
