/*
    core/cli/pinspot/.cpp - Ruth Command Line Interface for PinSpot
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

#include "cli/lightdesk.hpp"
#include "core/core.hpp"
#include "lightdesk/control.hpp"

namespace ruth {
using namespace lightdesk;

static struct arg_dbl *a_dance, *a_bass_mag_floor, *a_complexity_floor,
    *a_mag_floor;

static struct arg_lit *a_config, *a_help, *a_major_peak, *a_object_sizes,
    *a_stop, *a_stats, *a_test;
static struct arg_end *a_end;

static CSTR SECS = "<seconds>";
static CSTR FLOAT = "<float>";

static void *argtable[] = {
    a_config = arg_litn("c", "config", 0, 1, "display configurable values"),
    a_dance = arg_dbln("D", "dance", SECS, 0, 1, "start dance mode"),
    a_major_peak = arg_litn("M", "major-peak", 0, 1, "set major peak mode"),
    a_object_sizes = arg_litn("O", "object-sizes", 0, 1,
                              "display LightDesk related object sizes"),
    // secs = arg_dbln("t", "secs", FLOAT, 0, 1, nullptr),
    a_mag_floor =
        arg_dbln(nullptr, "mag-floor", FLOAT, 0, 1, "set i2s magnitude floor"),
    a_complexity_floor = arg_dbln(nullptr, "complexity-floor", FLOAT, 0, 1,
                                  "set i2s complexity floor"),
    a_bass_mag_floor = arg_dbln(nullptr, "bass-mag-floor", FLOAT, 0, 1,
                                "set i2s bass detection magnitude"),
    a_help = arg_litn("h", "help", 0, 1, "display help glossary"),
    a_stats = arg_litn(nullptr, "stats", 0, 1, "display LightDesk stats"),
    a_stop = arg_litn(nullptr, "stop", 0, 1, "stop and deallocate LightDesk"),
    a_test = arg_litn("T", "test", 0, 1, "special tests"),
    a_end = arg_end(12),
};

int LightDeskCli::execute(int argc, char **argv) {
  LightDeskControl_t *desk_ctrl = Core::lightDeskControl();
  auto rc = true;

  int nerrors = arg_parse(argc, argv, argtable);

  if (nerrors > 0) {
    arg_print_errors(stdout, a_end, "pinspot");
    return 1;
  }

  if (a_object_sizes->count > 0) {
    rc = LightDeskControl::objectSizes();
  }

  if (desk_ctrl == nullptr) {
    printf("LightDesk not enabled\n");
    return 1;
  }

  if (a_help->count > 0) {
    arg_print_syntax(stdout, argtable, "\n");
    arg_print_glossary(stdout, argtable, "  %-25s %s\n");
    rc = true;
  } else if (a_stats->count > 0) {
    rc = desk_ctrl->reportStats();
  } else if (a_config->count > 0) {
    rc = desk_ctrl->config();
  } else if (a_dance->count > 0) {
    const float secs = a_dance->dval[0];
    rc = desk_ctrl->dance(secs);
  } else if (a_major_peak->count > 0) {
    rc = desk_ctrl->majorPeak();
  } else if (a_stop->count > 0) {
    rc = desk_ctrl->stop();
  } else if (a_mag_floor->count > 0) {
    const float floor = (float)a_mag_floor->dval[0];
    rc = desk_ctrl->majorPeakMagFloor(floor);
  } else if (a_complexity_floor->count > 0) {
    const float floor = (float)a_complexity_floor->dval[0];
    rc = desk_ctrl->complexityFloor(floor);
  } else if (a_bass_mag_floor->count > 0) {
    const float floor = (float)a_bass_mag_floor->dval[0];
    rc = desk_ctrl->bassMagnitudeFloor(floor);
  } else if (a_test->count > 0) {
    printf("test not available\n");
    rc = false;
  } else {
    rc = false;
  }

  return invertReturnCode(rc);
}

void LightDeskCli::registerArgTable() {
  static esp_console_cmd_t cmd = {};
  cmd.command = "lightdesk";
  cmd.help = "Control the Light Desk";
  cmd.hint = nullptr;
  cmd.func = &execute;
  cmd.argtable = argtable;

  esp_console_cmd_register(&cmd);
}

}; // namespace ruth
