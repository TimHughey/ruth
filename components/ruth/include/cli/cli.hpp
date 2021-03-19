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

#include <esp_console.h>
#include <esp_vfs_dev.h>
#include <linenoise/linenoise.h>

#include "cli/binder.hpp"
#include "cli/ota.hpp"
#include "cli/random.hpp"
#include "cli/shell.hpp"
#include "local/types.hpp"

namespace ruth {

typedef class CLI CLI_t;
typedef const char *STRING;

class CLI {
public:
  CLI() { init(); };

  static bool remoteLine(MsgPayload_t *payload);
  bool running() const { return (_task.handle == nullptr) ? false : true; }

  void start() {
    if (_task.handle) {
      return;
    }

    // this (object) is passed as the data to the task creation and is
    // used by the static runTask method to call object specific run loop
    ::xTaskCreate(&runTask, "Rcli", _task.stackSize, this, _task.priority,
                  &_task.handle);
  }

private:
  void init();
  void initCommands();
  void loop();

  static void runLine(const char *line, int &ret);

  // Task implementation
  static void runTask(void *task_instance) {
    CLI_t *cli = (CLI_t *)task_instance;
    cli->loop();

    TaskHandle_t to_delete = cli->_task.handle;
    cli->_task.handle = nullptr;

    // return UART handling to default prior to console component usage
    esp_vfs_dev_uart_use_nonblocking(CONFIG_ESP_CONSOLE_UART_NUM);
    esp_console_deinit(); // free console component

    ESP_LOGI(pcTaskGetTaskName(nullptr), "handle[%p] flagged for delete",
             to_delete);

    vTaskDelete(to_delete);
  }

private:
  // commands
  BinderCli_t binder;
  OtaCli_t ota;
  ShellCli_t shell;
  RandomCli_t random;

  Task_t _task = {.handle = nullptr,
                  .data = nullptr,
                  .priority = 1, // allow reporting to continue
                  .stackSize = (5 * 1024)};

  const char *history_file_ = "/r/cli_hist.txt";
};

} // namespace ruth

#endif
