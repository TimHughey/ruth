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

static struct arg_dbl *dance, *a_bass_mag_floor, *a_roc_floor, *secs, *strobe;
static struct arg_end *end;
static struct arg_lit *dark, *fill, *a_help, *main, *a_major_peak, *ready,
    *stop, *stats, *test;
static struct arg_str *color, *fadeto;

static CSTR SECS = "<seconds>";
static CSTR FLOAT = "<float>";
static CSTR RGBW = "RR,GG,BB,WW";

static void *argtable[] = {
    fill = arg_litn(nullptr, "main", 0, 1, "target main pinspot"),
    main = arg_litn(nullptr, "fill", 0, 1, "target fill pinspot"),
    dance = arg_dbln("D", "dance", SECS, 0, 1, "start dance mode"),
    color = arg_strn("C", "color", RGBW, 0, 1, "set color"),
    dark = arg_litn("d", "dark", 0, 1, "set to dark"),
    ready = arg_litn(nullptr, "ready", 0, 1, "set ready"),
    fadeto = arg_strn("F", "fade-to", RGBW, 0, 1, "fade to specified color"),
    a_major_peak = arg_litn("M", "major-peak", 0, 1, "set major peak mode"),
    secs = arg_dbln("t", "secs", FLOAT, 0, 1, nullptr),
    a_roc_floor = arg_dbln(nullptr, "roc-floor", FLOAT, 0, 1,
                           "set fxMajorPeak rate of change threshold"),
    a_bass_mag_floor = arg_dbln(nullptr, "bass-mag-floor", FLOAT, 0, 1,
                                "set i2s bass detection magnitude"),
    a_help = arg_litn("h", "help", 0, 1, "display help"),
    strobe = arg_dbln("S", "strobe", FLOAT, 0, 1, "set strobe"),
    stats = arg_litn(nullptr, "stats", 0, 1, nullptr),
    stop = arg_litn(nullptr, "stop", 0, 1, nullptr),
    test = arg_litn("T", "test", 0, 1, nullptr),
    end = arg_end(12),
};

uint32_t LightDeskCli::convertHex(const char *str) {
  uint8_t red, green, blue, white = 0;
  uint32_t color = 0;
  auto matches =
      sscanf(str, "%2hhx,%2hhx,%2hhx,%2hhx", &red, &green, &blue, &white);

  if (matches < 1) {
    printf("warning: unable to parse color\n");
  }

  color = (red << 24) + (green << 16) + (blue << 8) + white;

  return color;
}

int LightDeskCli::execute(int argc, char **argv) {
  LightDeskControl_t *desk_ctrl = Core::lightDeskControl();
  auto rc = true;

  secs->dval[0] = 1.0;

  int nerrors = arg_parse(argc, argv, argtable);

  if (nerrors > 0) {
    arg_print_errors(stdout, end, "pinspot");
    return 1;
  }

  if (desk_ctrl == nullptr) {
    printf("LightDesk not enabled\n");
    return 1;
  }

  if (stats->count > 0) {
    rc = desk_ctrl->reportStats();
    return invertReturnCode(rc);
  }

  PinSpotFunction_t func = PINSPOT_MAIN;
  if (fill->count > 0) {
    func = PINSPOT_FILL;
  }

  if (a_help->count > 0) {
    arg_print_syntax(stdout, argtable, "\n");
    arg_print_glossary(stdout, argtable, "  %-25s %s\n");
    rc = true;
  } else if (dark->count > 0) {
    rc = desk_ctrl->dark();
  } else if (dance->count > 0) {
    const float secs = dance->dval[0];
    rc = desk_ctrl->dance(secs);
  } else if (color->count > 0) {
    uint32_t rgbw = convertHex(color->sval[0]);

    if (validate(STROBE)) {
      if (strobe->count > 0) {
        rc = desk_ctrl->color(func, rgbw, strobe->dval[0]);
      } else {
        rc = desk_ctrl->color(func, rgbw);
      }
    }
  } else if (fadeto->count > 0) {
    uint32_t rgbw = convertHex(fadeto->sval[0]);
    auto secs_val = secs->dval[0];

    rc = desk_ctrl->fadeTo(func, rgbw, secs_val);
  } else if (a_major_peak->count > 0) {
    rc = desk_ctrl->majorPeak();
  } else if (ready->count > 0) {
    rc = desk_ctrl->ready();
  } else if (stop->count > 0) {
    rc = desk_ctrl->stop();
  } else if (a_roc_floor->count > 0) {
    const float floor = (float)a_roc_floor->dval[0];
    rc = desk_ctrl->majorPeakRocFloor(floor);
  } else if (a_bass_mag_floor->count > 0) {
    const float floor = (float)a_bass_mag_floor->dval[0];
    rc = desk_ctrl->bassMagnitudeFloor(floor);
  } else if (test->count > 0) {
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

bool LightDeskCli::validate(Args_t arg) {
  float fval;
  switch (arg) {
  case STROBE:
    if (strobe->count > 0) {
      fval = strobe->dval[0];

      if ((fval < 0.0) && (fval > 1.00)) {
        printf("strobe must be >= 0.0 and <= 1.00 (a percentage)\n");
        return false;
      }
    }
  }

  return true;
}
}; // namespace ruth
