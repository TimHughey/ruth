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
#include <esp_vfs_dev.h>
#include <linenoise/linenoise.h>
#include <stdio.h>
#include <string.h>

#include "cli/cli.hpp"
#include "core/binder.hpp"
#include "external/ArduinoJson.h"

namespace ruth {

static struct arg_file *fw_file;
static struct arg_str *host, *path;
static struct arg_lit *mark_valid;
static struct arg_end *end;

static void *ota_argtable[] = {
    host = arg_strn("h", "host", "<hostname>", 0, 1,
                    "https://<HOSTNAME>/<path>/<file>"),
    path = arg_strn("p", "path", "<path to file>", 0, 1,
                    "https://<hostname>/<PATH>/<file>"),
    fw_file = arg_filen("f", "file", "<filename>", 0, 1,
                        "https://<hostname>/<path>/<FILE>"),
    mark_valid =
        arg_litn(nullptr, "mark-valid", 0, 1, "mark ota valid, if needed"),
    end = arg_end(4),
}; // namespace ruth

void CLI::init() {
  /* Drain stdout before reconfiguring it */
  fflush(stdout);
  fsync(fileno(stdout));

  /* Disable buffering on stdin */
  setvbuf(stdin, NULL, _IONBF, 0);

  /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
  esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,
                                            ESP_LINE_ENDINGS_CR);
  /* Move the caret to the beginning of the next line on '\n' */
  esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,
                                            ESP_LINE_ENDINGS_CRLF);

  /* Configure UART. Note that REF_TICK is used so that the baud rate remains
   * correct while APB frequency is changing in light sleep mode.
   */
  uart_config_t uart_config = {};
  uart_config.baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.source_clk = UART_SCLK_REF_TICK;

  /* Install UART driver for interrupt-driven reads and writes */
  ESP_ERROR_CHECK(
      uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0));
  ESP_ERROR_CHECK(uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config));

  /* Tell VFS to use UART driver */
  esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

  /* Initialize the console */
  esp_console_config_t console_config = {};
  console_config.max_cmdline_args = 8;
  console_config.max_cmdline_length = 256;

  ESP_ERROR_CHECK(esp_console_init(&console_config));
  esp_console_register_help_command();

  registerAllCommands();

  /* Configure linenoise line completion library */
  /* Enable multiline editing. If not set, long commands will scroll within
   * single line.
   */
  linenoiseSetMultiLine(1);

  /* Tell linenoise where to get command completions and hints */
  linenoiseSetCompletionCallback(&esp_console_get_completion);
  linenoiseSetHintsCallback((linenoiseHintsCallback *)&esp_console_get_hint);

  /* Set command history size */
  linenoiseHistorySetMaxLen(100);

  /* Don't return empty lines */
  linenoiseAllowEmpty(false);

  printf("\nReady.\n");
}

void CLI::loop() {
  int ret = 0;
  for (;;) {
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

    /* Try to run the command */
    esp_err_t err = esp_console_run(line, &ret);
    if (err == ESP_ERR_NOT_FOUND) {
      ret = 1;
      printf("command not found: %s\n", line);
    } else if (err == ESP_ERR_INVALID_ARG) {
      ret = 1;
      // command was empty
    } else if (err == ESP_OK && ret != ESP_OK) {
      // printf("command failed, error code[0x%x]\n", ret);
    } else if (err != ESP_OK) {
      printf("internal error: %s\n", esp_err_to_name(err));
    }
    /* linenoise allocates line buffer on the heap, so need to free it */
    linenoiseFree(line);
  }
}

int CLI::otaCommand(int argc, char **argv) {
  mark_valid->count = 0;
  host->sval[0] = Binder::otaHost();
  path->sval[0] = Binder::otaPath();
  fw_file->filename[0] = Binder::otaFile();

  int nerrors = arg_parse(argc, argv, ota_argtable);

  if (nerrors > 0) {
    return 1;
  }

  if (mark_valid->count > 0) {
    OTA::markPartitionValid();
  } else {

    TextBuffer<128> uri;

    uri.printf("https://%s/%s/%s", host->sval[0], path->sval[0],
               fw_file->filename[0]);

    StaticJsonDocument<128> doc;
    JsonObject root = doc.to<JsonObject>();
    root["uri"] = uri.c_str();

    TextBuffer<128> msgpack;
    auto bytes = serializeMsgPack(doc, msgpack.data(), msgpack.capacity());
    msgpack.forceSize(bytes);

    OTA::queuePayload(msgpack.c_str());
  }

  return 0;
}

void CLI::registerOtaCommand() {
  static esp_console_cmd_t cmd = {};
  cmd.command = "ota";
  cmd.help = "Perform an OTA update or mark the ota partition valid";
  cmd.hint = NULL;
  cmd.func = &otaCommand;
  cmd.argtable = ota_argtable;

  esp_console_cmd_register(&cmd);
}
} // namespace ruth
