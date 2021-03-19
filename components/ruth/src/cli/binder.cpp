/*
    cli/binder.cpp - Ruth CLI Binder
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
#include <stdio.h>
#include <string.h>

#include "cli/binder.hpp"

namespace ruth {

static struct arg_end *end;

static struct arg_lit *cp, *ls, *print, *rm, *versions;

static void *argtable[] = {
    cp = arg_litn("c", "cp", 0, 1, "cp <firmware> /r/binder_0.mp"),
    ls = arg_litn("l", "ls", 0, 1, "ls /r"),
    print = arg_litn("p", "pr", 0, 1, "print active binder"),
    rm = arg_litn("r", "rm", 0, 1, "rm /r/binder_0.mp"),
    versions = arg_litn("v", "ve", 0, 1, "list available versions"),
    end = arg_end(10),
};

int BinderCli::execute(int argc, char **argv) {
  Binder_t *binder = Binder::instance();

  cp->count = 0;
  ls->count = 0;
  rm->count = 0;

  int nerrors = arg_parse(argc, argv, argtable);

  if (nerrors > 0) {
    printf("\nbinder: invalid options\n");
    return 1;
  }

  if (cp->count > 0) {
    auto bytes = binder->copyToFilesystem();
    if (bytes > 0) {
      printf("binder: wrote %d bytes\n", bytes);
      return 0;
    } else {
      printf("\nbinder: write to filesystem failed\n");
      return 1;
    }
  }

  if (ls->count > 0) {
    return binder->ls();
  }

  if (rm->count > 0) {
    return binder->rm();
  }

  if (versions->count > 0) {
    return binder->versions();
  }

  if (print->count > 0) {
    Binder::PrettyJson buff;
    auto bytes = binder->pretty(buff);

    printf("used: %4d total: %4d (%0.1f%%)\n", bytes, buff.capacity(),
           buff.usedPrecent());
    printf("%s\n", buff.c_str());
  }

  return 1;
}

void BinderCli::registerArgTable() {
  static esp_console_cmd_t cmd = {};
  cmd.command = "binder";
  cmd.help = "Binder administration";
  cmd.hint = NULL;
  cmd.func = &execute;
  cmd.argtable = argtable;

  esp_console_cmd_register(&cmd);
}

}; // namespace ruth
