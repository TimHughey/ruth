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

static Binder_t *binder = nullptr;

static struct arg_end *binder_end, *ota_end, *rm_end;

static struct arg_lit *binder_cp, *binder_ls, *binder_print, *binder_rm,
    *binder_versions;

static struct arg_file *fw_file;
static struct arg_str *host, *path;
static struct arg_lit *mark_valid;

// static struct arg_lit *rm_help;
static struct arg_file *rm_file;

static void *binder_argtable[] = {
    binder_cp = arg_litn("c", "cp", 0, 1, "cp <firmware> /r/binder_0.mp"),
    binder_ls = arg_litn("l", "ls", 0, 1, "ls /r"),
    binder_print = arg_litn("p", "pr", 0, 1, "print active binder"),
    binder_rm = arg_litn("r", "rm", 0, 1, "rm /r/binder_0.mp"),
    binder_versions = arg_litn("v", "ve", 0, 1, "list available versions"),
    binder_end = arg_end(10),
};

static void *ota_argtable[] = {
    host = arg_strn("h", "host", "<hostname>", 0, 1,
                    "https://<HOSTNAME>/<path>/<file>"),
    path = arg_strn("p", "path", "<path to file>", 0, 1,
                    "https://<hostname>/<PATH>/<file>"),
    fw_file = arg_filen("f", "file", "<filename>", 0, 1,
                        "https://<hostname>/<path>/<FILE>"),
    mark_valid =
        arg_litn(nullptr, "mark-valid", 0, 1, "mark ota valid, if needed"),
    ota_end = arg_end(4),
};

static void *rm_argtable[] = {
    // rm_help = arg_litn("h", "help", 0, 1, nullptr),
    rm_file = arg_filen(NULL, NULL, "<file>", 1, 1, "file to delete"),
    rm_end = arg_end(1),
};

int CLI::commandBinder(int argc, char **argv) {
  binder_cp->count = 0;
  binder_ls->count = 0;
  binder_rm->count = 0;

  int nerrors = arg_parse(argc, argv, binder_argtable);

  if (nerrors > 0) {
    printf("\nbinder: invalid options\n");
    return 1;
  }

  if (binder_cp->count > 0) {
    auto bytes = binder->copyToFilesystem();
    if (bytes > 0) {
      printf("binder: wrote %d bytes\n", bytes);
      return 0;
    } else {
      printf("\nbinder: write to filesystem failed\n");
      return 1;
    }
  }

  if (binder_ls->count > 0) {
    return binder->ls();
  }

  if (binder_rm->count > 0) {
    return binder->rm();
  }

  if (binder_versions->count > 0) {
    return binder->versions();
  }

  if (binder_print->count > 0) {
    BinderPrettyJson_t buff;
    auto bytes = binder->pretty(buff);

    printf("used: %4d total: %4d (%0.1f%%)\n", bytes, buff.capacity(),
           buff.usedPrecent());
    printf("%s\n", buff.c_str());
  }

  return 1;
}

int CLI::commandOta(int argc, char **argv) {
  mark_valid->count = 0;
  host->sval[0] = binder->otaHost();
  path->sval[0] = binder->otaPath();
  fw_file->filename[0] = binder->otaFile();

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

int CLI::commandRm(int argc, char **argv) {
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
  registerBinderCommand();
  registerClearCommand();
  registerDateCommand();
  registerDiceRollStatsCommand();
  registerExitCommand();
  lightdesk.init();
  registerLsCommand();
  registerOtaCommand();
  registerRestartCommand();
  registerRmCommand();
}

void CLI::loop() {
  binder = Binder::instance();

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

void CLI::registerBinderCommand() {
  static esp_console_cmd_t cmd = {};
  cmd.command = "binder";
  cmd.help = "Binder administration";
  cmd.hint = NULL;
  cmd.func = &commandBinder;
  cmd.argtable = binder_argtable;

  esp_console_cmd_register(&cmd);
}

void CLI::registerOtaCommand() {
  static esp_console_cmd_t cmd = {};
  cmd.command = "ota";
  cmd.help = "Perform an OTA update or mark the ota partition valid";
  cmd.hint = NULL;
  cmd.func = &commandOta;
  cmd.argtable = ota_argtable;

  esp_console_cmd_register(&cmd);
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
