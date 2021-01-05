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

namespace ruth {
using namespace lightdesk;

static struct arg_dbl *dance, *secs, *strobe;
static struct arg_end *end;
static struct arg_lit *dark, *fill, *main, *pause, *resume, *stats, *test;
static struct arg_str *color, *fadeto;

static CSTR DANCE = "<seconds>";
static CSTR FLOAT = "<float>";
static CSTR RGBW = "RR,GG,BB,WW";

static void *argtable[] = {
    fill = arg_litn(nullptr, "main", 0, 1, "target main pinspot"),
    main = arg_litn(nullptr, "fill", 0, 1, "target fill pinspot"),
    dance = arg_dbln("D", "dance", DANCE, 0, 1, "start dance mode"),
    color = arg_strn("C", "color", RGBW, 0, 1, "set color"),
    dark = arg_litn("d", "dark", 0, 1, "set to dark"),
    fadeto = arg_strn("F", "fade-to", RGBW, 0, 1, "fade to specified color"),
    secs = arg_dbln("t", "secs", FLOAT, 0, 1, nullptr),
    strobe = arg_dbln("S", "strobe", FLOAT, 0, 1, "set strobe"),
    stats = arg_litn(nullptr, "stats", 0, 1, nullptr),
    pause = arg_litn(nullptr, "pause", 0, 1, nullptr),
    resume = arg_litn(nullptr, "resume", 0, 1, nullptr),
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
  LightDesk_t *lightdesk = LightDesk::instance();

  auto rc = 0;

  secs->dval[0] = 1.0;

  int nerrors = arg_parse(argc, argv, argtable);

  if (nerrors > 0) {
    arg_print_errors(stdout, end, "pinspot");
    return rc;
  }

  PinSpotFunction_t func = PINSPOT_MAIN;
  if (fill->count > 0) {
    func = PINSPOT_FILL;
  }

  if (dark->count > 0) {
    lightdesk->dark();
  } else if (dance->count > 0) {
    float secs = dance->dval[0];

    lightdesk->dance(secs);
  } else if (color->count > 0) {
    uint32_t rgbw = convertHex(color->sval[0]);

    if (validate(STROBE)) {
      if (strobe->count > 0) {
        lightdesk->color(func, rgbw, strobe->dval[0]);
      } else {
        lightdesk->color(func, rgbw);
      }
    }
  } else if (fadeto->count > 0) {
    uint32_t rgbw = convertHex(fadeto->sval[0]);
    auto secs_val = secs->dval[0];

    lightdesk->fadeTo(func, rgbw, secs_val);
  } else if (stats->count > 0) {
    reportStats(lightdesk);
  } else if (pause->count > 0) {
    rc = pauseDesk(lightdesk);
  } else if (resume->count > 0) {
    rc = resumeDesk(lightdesk);
  } else if (test->count > 0) {
    printf("test not implemented at present\n");
    rc = 1;
  } else {
    rc = 1;
  }

  return rc;
}

int LightDeskCli::pauseDesk(LightDesk_t *lightdesk) {
  lightdesk->pause();

  return 0;
}

int LightDeskCli::resumeDesk(LightDesk_t *lightdesk) {
  lightdesk->resume();

  return 0;
}

void LightDeskCli::reportStats(LightDesk_t *lightdesk) {
  const LightDeskStats_t &stats = lightdesk->stats();
  const DmxStats_t &dmx = stats.dmx;

  printf("\n");
  printf("lightdesk:      mode=%s\n", stats.mode);

  printf("\n");
  printf("lightdesk_fx:   basic=%llu\n", stats.fx.basic);
  printf("                interval_default=%3.2f\n", stats.fx.interval_default);
  printf("                interval=%3.2f\n", stats.fx.interval);
  printf("                active=0x%02x\n", stats.fx.active);
  printf("                next=0x%02x\n", stats.fx.next);
  printf("                object_size=%u\n", stats.fx.object_size);

  printf("\n");

  printf("\ndmx:\t%02.02ffps\n\tframe=%lldµs"
         "\n\tbusy_wait=%lld\n\tnotify_failures=%lld\n\t"
         "tx_elapsed=%02.02fms\n\ttx_short_frames=%lld\n",
         dmx.fps, dmx.frame_us, dmx.busy_wait, dmx.notify_failures,
         dmx.tx_elapsed, dmx.tx_short_frames);

  const uint32_t last_pinspot = static_cast<uint32_t>(PINSPOT_FILL);

  for (uint32_t i = 0; i <= last_pinspot; i++) {
    printf("\n");
    const PinSpotStats_t &pinspot = stats.pinspot[i];

    printf("pinspot %02d: notify: count=%llu retries=%llu failures=%llu\n", i,
           pinspot.notify.count, pinspot.notify.retries,
           pinspot.notify.failures);
    printf("            object_size=%u\n", pinspot.object_size);
  }

  printf("\n");
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
