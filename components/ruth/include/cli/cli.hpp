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
#include "cli/lightdesk.hpp"
#include "cli/ota.hpp"
#include "core/ota.hpp"
#include "local/types.hpp"
#include "misc/datetime.hpp"
#include "misc/restart.hpp"
#include "net/network.hpp"
#include "protocols/dmx.hpp"
#include "protocols/mqtt.hpp"

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

  static int commandBinder(int argc, char **argv);

  static int commandClear(int argc, char **argv) {
    linenoiseClearScreen();
    return 0;
  }

  static int commandDate(int argc, char **argv) {
    printf("%s\n", DateTime().c_str());
    return 0;
  }

  static int commandDiceStats(int argc, char **argv) {
    printDiceRollStats();
    return 0;
  }

  static int commandExit(int argc, char **argv) { return 255; }
  static int commandLs(int argc, char **argv) {
    return Binder::instance()->ls();
  }

  static int commandReboot(int argc, char **argv) {
    Restart("cli initiated reboot").now();

    return 0;
  }
  static int commandRm(int argc, char **argv);

  static uint32_t convertHex(const char *str);

  void registerClearCommand() {
    static esp_console_cmd_t cmd = {};
    cmd.command = "c";
    cmd.help = "Clears the screen";
    cmd.hint = NULL;
    cmd.func = &commandClear;

    esp_console_cmd_register(&cmd);
  }

  void registerDateCommand() {
    static esp_console_cmd_t cmd = {};
    cmd.command = "date";
    cmd.help = "Display the current date and time";
    cmd.hint = NULL;
    cmd.func = &commandDate;

    esp_console_cmd_register(&cmd);
  }

  void registerDiceRollStatsCommand() {
    static esp_console_cmd_t cmd = {};
    cmd.command = "dicestats";
    cmd.help = "Display the current die roll stats";
    cmd.hint = NULL;
    cmd.func = &commandDiceStats;

    esp_console_cmd_register(&cmd);
  }

  void registerExitCommand() {
    static esp_console_cmd_t cmd = {};
    cmd.command = "exit";
    cmd.help = "Exit the Command Line Interface";
    cmd.hint = NULL;
    cmd.func = &commandExit;

    esp_console_cmd_register(&cmd);
  }

  void registerLsCommand() {
    static esp_console_cmd_t cmd = {};
    cmd.command = "ls";
    cmd.help = "List files";
    cmd.hint = NULL;
    cmd.func = &commandLs;

    esp_console_cmd_register(&cmd);
  }

  void registerRestartCommand() {
    static esp_console_cmd_t cmd = {};
    cmd.command = "reboot";
    cmd.help = "Reboot Ruth immediately";
    cmd.hint = NULL;
    cmd.func = &commandReboot;

    esp_console_cmd_register(&cmd);
  }

  void registerRmCommand() {
    static esp_console_cmd_t cmd = {};
    cmd.command = "rm";
    cmd.help = "Remove (unlink) a file";
    cmd.hint = NULL;
    cmd.func = &commandRm;

    esp_console_cmd_register(&cmd);
  }

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
  LightDeskCli_t lightdesk;
  OtaCli_t ota;

  Task_t _task = {.handle = nullptr,
                  .data = nullptr,
                  .priority = 1, // allow reporting to continue
                  .stackSize = (5 * 1024)};

  const char *history_file_ = "/r/cli_hist.txt";
};

} // namespace ruth

#endif
