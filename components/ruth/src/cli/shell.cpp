/*
    cli/shell.cpp - Ruth CLI for Shell-like commands
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
#include <linenoise/linenoise.h>
#include <stdio.h>
#include <string.h>

#include "cli/shell.hpp"
#include "core/binder.hpp"
#include "misc/datetime.hpp"
#include "misc/restart.hpp"

namespace ruth {

static struct arg_end *end;

static struct arg_file *rm_file;

static void *argtable[] = {
    rm_file = arg_filen(NULL, NULL, "<file>", 1, 1, "file to delete"),
    end = arg_end(3),
};

int ShellCli::executeClear(int argc, char **argv) {
  linenoiseClearScreen();
  return 0;
}

int ShellCli::executeDate(int argc, char **argv) {
  printf("%s\n", DateTime().c_str());
  return 0;
}

int ShellCli::executeLs(int argc, char **argv) {
  return Binder::instance()->ls();
}

int ShellCli::executeReboot(int argc, char **argv) {
  Restart("cli initiated reboot", __PRETTY_FUNCTION__).now();

  return 0;
}

int ShellCli::executeRm(int argc, char **argv) {
  Binder_t *binder = Binder::instance();
  auto rc = 0;
  TextBuffer<35> rm_path;

  int nerrors = arg_parse(argc, argv, argtable);

  if (nerrors > 0) {
    arg_print_errors(stdout, end, "rm");
    rc = 1;
    goto finish;
  }

  rm_path.printf("%s/%s", binder->basePath(), rm_file->filename[0]);
  rc = binder->rm(rm_path.c_str());

finish:
  return rc;
}

void ShellCli::registerArgTable() {
  static esp_console_cmd_t clear = {};
  clear.command = "c";
  clear.help = "Clears the screen";
  clear.hint = NULL;
  clear.func = &executeClear;
  esp_console_cmd_register(&clear);

  static esp_console_cmd_t date = {};
  date.command = "date";
  date.help = "Display the current date and time";
  date.hint = NULL;
  date.func = &executeDate;
  esp_console_cmd_register(&date);

  static esp_console_cmd_t exit = {};
  exit.command = "exit";
  exit.help = "Exit the Command Line Interface";
  exit.hint = NULL;
  exit.func = &executeExit;
  esp_console_cmd_register(&exit);

  static esp_console_cmd_t ls = {};
  ls.command = "ls";
  ls.help = "List files";
  ls.hint = NULL;
  ls.func = &executeLs;
  esp_console_cmd_register(&ls);

  static esp_console_cmd_t rm = {};
  rm.command = "rm";
  rm.help = "Remove (unlink) a file";
  rm.hint = NULL;
  rm.func = &executeRm;
  esp_console_cmd_register(&rm);

  static esp_console_cmd_t reboot = {};
  reboot.command = "reboot";
  reboot.help = "Reboot Ruth immediately";
  reboot.hint = NULL;
  reboot.func = &executeReboot;
  esp_console_cmd_register(&reboot);
}

} // namespace ruth
