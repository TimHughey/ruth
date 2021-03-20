/*
    Ruth
    Copyright (C) 2021  Tim Hughey

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

#include "cli/dmx.hpp"

namespace ruth {
namespace cli {

static struct arg_end *end;

static struct arg_lit *fps;

static void *argtable[] = {
    fps = arg_litn("f", "fps", 0, 1, "instantaneous fps"),
    end = arg_end(5),
};

int Dmx::execute(int argc, char **argv) {
  int rc = 0;
  ruth::Dmx *dmx = ruth::Dmx::instance();

  fps->count = 0;

  int nerrors = arg_parse(argc, argv, argtable);

  if (nerrors > 0) {
    printf("\ndmx: invalid option\n");
    rc = 1;
    goto finished;
  }

  if (fps->count > 0) {
    printf("fps: %5.2f\n", dmx->framesPerSecond());
    goto finished;
  }

finished:

  return rc;
}

void Dmx::registerArgTable() {
  static esp_console_cmd_t cmd = {};
  cmd.command = "dmx";
  cmd.help = "DMX control and stats";
  cmd.hint = NULL;
  cmd.func = &execute;
  cmd.argtable = argtable;

  esp_console_cmd_register(&cmd);
}
} // namespace cli
}; // namespace ruth
