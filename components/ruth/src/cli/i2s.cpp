/*
    cli/i2s.cpp - Ruth CLI for I2s
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

#include "cli/i2s.hpp"
#include "protocols/i2s.hpp"

namespace ruth {

I2s_t *_i2s = nullptr;

static struct arg_end *a_end;
static struct arg_int *a_print_secs;
static struct arg_lit *a_i2s, *a_print, *a_print_freq_bins, *a_stop;

static void *argtable[] = {
    a_i2s = arg_litn("i", nullptr, 0, 1, "init i2s"),
    a_print = arg_litn("p", nullptr, 0, 1, "print samples"),
    a_print_secs = arg_intn(nullptr, "secs", "<scalar>", 0, 1,
                            "print for specified seconds"),
    a_print_freq_bins =
        arg_litn("F", "freq-bins", 0, 1, "print FFT frequency bins"),
    a_stop = arg_litn("s", nullptr, 0, 1, "stop printing samples"),
    a_end = arg_end(3),
};

// #define AC(X) (&(X->count))
// static int *a_counts[] = {&(a_i2s->count)};

int I2sCli::execute(int argc, char **argv) {
  int rc = 0;

  int nerrors = arg_parse(argc, argv, argtable);

  auto any = a_i2s->count + a_print->count + a_stop->count +
             a_print_freq_bins->count + a_print_secs->count;

  if ((nerrors == 0) && (any > 0)) {
    if (_i2s == nullptr) {
      _i2s = new I2s();
      _i2s->start();
    }

    if (a_print_secs->count > 0) {
      const uint32_t secs = a_print_secs->ival[0];
      _i2s->setPrintSeconds(secs);
    }

    if (a_print->count > 0) {
      _i2s->samplePrint();
    }

    if ((a_stop->count > 0) && (_i2s)) {
      _i2s->sampleStopPrint();
    }

    if (a_print_freq_bins->count > 0) {
      _i2s->printFrequencyBins();
    }

  } else {
    arg_print_errors(stdout, a_end, "i2s");
    rc = 1;
  }

  return rc;
}

void I2sCli::registerArgTable() {
  static esp_console_cmd_t i2s = {};
  i2s.command = "i2s";
  i2s.help = "I2s control";
  i2s.hint = NULL;
  i2s.func = &execute;
  i2s.argtable = argtable;
  esp_console_cmd_register(&i2s);
}

} // namespace ruth
