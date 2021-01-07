/*
    core/cli.cpp - Ruth Command Line Interface
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

#include <argtable3/argtable3.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_system.h>
#include <stdio.h>
#include <string.h>

#include "cli/cli.hpp"
#include "core/binder.hpp"
#include "external/ArduinoJson.h"
#include "readings/text.hpp"

namespace ruth {

using namespace lightdesk;

static struct arg_end *rm_end;

// static struct arg_lit *rm_help;
static struct arg_file *rm_file;

static void *rm_argtable[] = {
    // rm_help = arg_litn("h", "help", 0, 1, nullptr),
    rm_file = arg_filen(NULL, NULL, "<file>", 1, 1, "file to delete"),
    rm_end = arg_end(1),
};

int CLI::commandRm(int argc, char **argv) {
  Binder_t *binder = Binder::instance();
  auto rc = 0;
  TextBuffer<35> rm_path;

  int nerrors = arg_parse(argc, argv, rm_argtable);

  if (nerrors > 0) {
    arg_print_errors(stdout, rm_end, "rm");
    rc = 1;
    goto finish;
  }

  rm_path.printf("%s/%s", binder->basePath(), rm_file->filename[0]);
  rc = binder->rm(rm_path.c_str());

finish:
  return rc;
}

void CLI::init() {
  fflush(stdout); // drain stdout before adjusting configuration
  fsync(fileno(stdout));

  setvbuf(stdin, NULL, _IONBF, 0); // no buffering of stdin

  // Minicom, screen, idf_monitor send CR when ENTER key is pressed
  esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,
                                            ESP_LINE_ENDINGS_CR);
  // Move the cursor to the beginning of the next line on '\n'
  esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,
                                            ESP_LINE_ENDINGS_CRLF);

  uart_config_t uart_config = {};
  uart_config.baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.source_clk = UART_SCLK_REF_TICK; // APB skews during light sleep

  // install UART driver for interrupt-driven reads and writes
  uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);
  uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config);
}

void CLI::initCommands() {
  esp_console_register_help_command();
  binder.init();
  registerClearCommand();
  registerDateCommand();
  registerDiceRollStatsCommand();
  registerExitCommand();
  lightdesk.init();
  ota.init();
  registerLsCommand();
  registerRestartCommand();
  registerRmCommand();
}

void CLI::loop() {

  // tell VFS to use the UART driver
  esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

  // initialize the console
  esp_console_config_t console_config = {};
  console_config.max_cmdline_args = 15;
  console_config.max_cmdline_length = 256;

  ESP_ERROR_CHECK(esp_console_init(&console_config));
  initCommands();

  linenoiseSetMultiLine(1); // prevent scrolling of long commands

  linenoiseSetCompletionCallback(&esp_console_get_completion);
  linenoiseSetHintsCallback((linenoiseHintsCallback *)&esp_console_get_hint);

  linenoiseHistorySetMaxLen(100);

  linenoiseAllowEmpty(false);

  printf("\nReady.\n");

  int ret = 0;
  for (auto run = true; run;) {
    TextBuffer<28> prompt;
    prompt.printf("%s [%d] %% ", Net::hostname(), ret);

    /* Get a line using linenoise.
     * The line is returned when ENTER is pressed.
     */
    char *line = linenoise(prompt);
    if (line == NULL) { /* Break on EOF or error */
      continue;
    }
    /* Add the command to the history if not empty*/
    if (strlen(line) > 0) {
      linenoiseHistoryAdd(line);
    }

    CLI::runLine(line, ret);

    if (ret == 255) {
      // exit the command line interface
      run = false;
    }

    /* linenoise allocates line buffer on the heap, so need to free it */
    linenoiseFree(line);
  }
}

bool CLI::remoteLine(MsgPayload_t *remote_line) {
  auto rc = false;
  StaticJsonDocument<256> doc;

  auto payload_err = deserializeMsgPack(doc, remote_line->payload());

  if (!payload_err) {
    const char *line = doc["text"];

    int ret;
    runLine(line, ret);

    if (ret == 0) {
      rc = true;
    } else {
      Text::rlog("rc[%d] from %s", rc, line);
    }
  }

  return rc;
}

void CLI::runLine(const char *line, int &ret) {
  esp_err_t err = esp_console_run(line, &ret);
  if (err == ESP_ERR_NOT_FOUND) {
    ret = 1;
    printf("unknown command: \"%s\"\n", line);
  } else if (err == ESP_ERR_INVALID_ARG) {
    ret = 1;
  } else if (err != ESP_OK) {
    printf("internal error: %s\n", esp_err_to_name(err));
    ret = -1;
  }
}

} // namespace ruth
