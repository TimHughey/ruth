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
static struct arg_lit *dark, *fill, *main, *ready, *stop, *stats, *test;
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
    ready = arg_litn(nullptr, "ready", 0, 1, "set ready"),
    fadeto = arg_strn("F", "fade-to", RGBW, 0, 1, "fade to specified color"),
    secs = arg_dbln("t", "secs", FLOAT, 0, 1, nullptr),
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
  auto rc = 0;

  secs->dval[0] = 1.0;

  int nerrors = arg_parse(argc, argv, argtable);

  if (nerrors > 0) {
    arg_print_errors(stdout, end, "pinspot");
    return rc;
  }

  if (stats->count > 0) {
    if (LightDesk::isRunning()) {
      LightDesk_t *lightdesk = LightDesk::instance();
      reportStats(lightdesk);
    } else {
      printf("LightDesk unavailable\n");
      rc = 1;
      return rc;
    }
  }

  LightDesk_t *lightdesk = LightDesk::instance();

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
  } else if (ready->count > 0) {
    lightdesk->ready();
    rc = 0;
  } else if (stop->count > 0) {
    lightdesk->stop();
    LightDesk::cleanUp();
    rc = 0;
  } else if (test->count > 0) {
    printf("test not available\n");
    rc = 1;
  } else {
    rc = 1;
  }

  return rc;
}

void LightDeskCli::reportStats(LightDesk_t *lightdesk) {
  static const char *indent = "                ";
  const LightDeskStats_t &stats = lightdesk->stats();
  const DmxStats_t &dmx = stats.dmx;
  const LightDeskFxStats_t &fx = stats.fx;

  printf("\n");
  printf("lightdesk:      mode=%s object_size=%u\n", stats.mode,
         stats.object_size);

  printf("\n");
  printf("lightdesk_fx:   basic=%llu active=%s next=%s prev=%s\n", fx.fx.basic,
         fxDesc(fx.fx.active), fxDesc(fx.fx.next), fxDesc(fx.fx.prev));

  printf("%sinterval curr=%4.2fs min=%4.2fs max=%4.2fs base=%4.2fs\n", indent,
         fx.interval.current, fx.interval.min, fx.interval.max,
         fx.interval.base);
  printf("%sobject_size=%u\n", indent, fx.object_size);

  printf("\n");

  const float frame_ms = (float)dmx.frame.us / 1000.f;

  printf("dmx:            %02.02ffps frame=%5.3fms shorts=%llu ", dmx.fps,
         frame_ms, dmx.frame.shorts);
  printf("frame_update: curr=%lluµs min=%lluµs max=%lluµs\n",
         dmx.frame.update.curr, dmx.frame.update.min, dmx.frame.update.max);
  printf("%stx curr=%02.02fms min=%02.02fms max=%02.02fms\n", indent,
         dmx.tx.curr, dmx.tx.min, dmx.tx.max);
  printf("%sbusy_wait=%lld object_size=%u\n", indent, dmx.busy_wait,
         dmx.object_size);

  const uint32_t last_pinspot = static_cast<uint32_t>(PINSPOT_FILL);

  for (uint32_t i = 0; i <= last_pinspot; i++) {
    printf("\n");
    const PinSpotStats_t &pinspot = stats.pinspot[i];
    TextBuffer<16> name;
    name.printf("pinspot %02d:", i);

    printf("%-16snotify count=%llu retries=%llu failures=%llu\n", name.c_str(),
           pinspot.notify.count, pinspot.notify.retries,
           pinspot.notify.failures);
    printf("%sobject_size=%u\n", indent, pinspot.object_size);
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
