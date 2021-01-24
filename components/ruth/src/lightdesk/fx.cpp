/*
    lightdesk/fx.cpp - Ruth Light Desk Fx
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

#include <esp_log.h>
#include <math.h>

#include "lightdesk/fx.hpp"

namespace ruth {
namespace lightdesk {

LightDeskFx::LightDeskFx(PinSpot_t *main, PinSpot_t *fill, I2s_t *i2s)
    : _main(main), _fill(fill), _i2s(i2s){};

void LightDeskFx::execute(Fx_t next_fx) {
  _fx_next = next_fx;

  execute();
}

bool LightDeskFx::execute(bool *finished_ptr) {
  bool finished = true;

  switch (_fx_next) {

  case fxNone:
    break;

  case fxPrimaryColorsCycle:
    primaryColorsCycle();
    break;

  case fxWashedSound:
    soundWashed();
    break;

  case fxFullSpectrumCycle:
    fullSpectrumCycle();
    break;

  case fxWhiteFadeInOut:
    whiteFadeInOut();
    break;

  case fxFastStrobeSound:
    soundFastStrobe();
    break;

  case fxSimpleStrobe:
    simpleStrobe();
    break;

  case fxCrossFadeFast:
    finished = crossFadeFast();
    break;

  case fxColorBars:
    finished = colorBars();
    break;

  case fxMajorPeak:
    finished = majorPeak();
    break;

  default:
    basic(_fx_next);
    break;
  }

  // when finished set internal state to reflect no active fx
  if (finished) {
    _fx_prev = (_fx_next != fxNone) ? _fx_next : _fx_prev;
    _fx_active = fxNone;
    _fx_next = fxNone;
    _fx_active_count = 0;

    if (finished_ptr) {
      *finished_ptr = finished;
    }
  } else {
    // when more to do for this fx set next fx to the active fx
    _fx_next = _fx_active;
  }

  return finished;
}

void LightDeskFx::start() { execute(fxColorBars); }

bool IRAM_ATTR LightDeskFx::withinRange(const float val, const float low,
                                        const float high, const float mag,
                                        const float mag_floor) const {
  bool rc = false;

  if ((mag >= mag_floor) && ((val >= low) && (val < high))) {
    rc = true;
  }

  return rc;
}

//
// Fx Functions
//

void LightDeskFx::basic(Fx_t fx) {
  _stats.fx.basic++;

  _main->autoRun(fx); // always run the choosen fx on main

  const auto roll = roll1D6();

  // make basic Fx more interesting
  switch (roll) {
  case 1:
    _fill->autoRun(fx);
    break;

  case 2:
    _fill->color(Color::red(), 0.75f);
    break;

  case 3:
    _fill->color(Color::blue(), 0.75f);
    break;

  case 4:
    _fill->color(Color::green(), 0.75f);
    break;

  case 5:
    _fill->autoRun(fxFastStrobeSound);
    break;

  case 6:
    _fill->autoRun(fxColorCycleSound);
    break;
  }

  // limit the duration of the same basic fx
  if (fx == _fx_prev) {
    intervalReduceTo(0.10f);
  } else {
    intervalReduceTo(0.27f);
  }
}

bool LightDeskFx::colorBars() {
  auto finished = false;
  auto &count = fxActiveCount();

  if (_fx_active != fxColorBars) {
    count = 10;
  }

  _fx_active = fxColorBars;

  PinSpot_t *pinspot = nullptr;

  FaderOpts_t fade{.origin = Color::black(),
                   .dest = Color::black(),
                   .travel_secs = 0.8f,
                   .use_origin = true};

  const auto pinspot_select = count % 2;
  if (pinspot_select == 0) {
    // evens we act on the main pinspot
    pinspot = _main;
  } else {
    pinspot = _fill;
  }

  switch (count) {
  case 1:
    finished = true;
    break;

  case 2:
    _main->color(Color::black());
    _fill->color(Color::black());
    break;

  case 3:
  case 4:
    fade.origin = Color::white();
    break;

  case 5:
  case 6:
    fade.origin = Color::blue();
    break;

  case 7:
  case 8:
    fade.origin = Color::green();
    break;

  case 9:
  case 10:
    fade.origin = Color::red();
    break;
  }

  if (count > 2) {
    pinspot->fadeTo(fade);
  }

  count--;

  _fx_interval = fade.travel_secs + 0.2f;

  return finished;
}

bool LightDeskFx::crossFadeFast() {
  auto finished = true;
  auto &count = fxActiveCount();
  const auto interval = 1.5f;

  FaderOpts_t main_fade{.origin = Color::randomize(),
                        .dest = Color::randomize(),
                        .travel_secs = interval - 0.2f,
                        .use_origin = false};

  FaderOpts_t fill_fade{.origin = Color::randomize(),
                        .dest = Color::randomize(),
                        .travel_secs = interval - 0.2f,
                        .use_origin = false};

  if (_fx_active != fxCrossFadeFast) {
    intervalReset();
    count = (intervalPercent(0.75) / interval);
    main_fade.use_origin = true;
    fill_fade.use_origin = true;
  }

  _fx_active = fxCrossFadeFast;

  _main->fadeTo(main_fade);
  _fill->fadeTo(fill_fade);

  count--;
  if (count > 0) {
    finished = false;
  }

  _fx_interval = interval;

  return finished;
}

void LightDeskFx::fullSpectrumCycle() {
  _main->autoRun(_fx_next);
  _fill->color(Color(128, 0, 0, 0), 0.75);
  intervalReduceTo(randomPercent(15, 25));
}

bool IRAM_ATTR LightDeskFx::majorPeak() {
  bool finished = false;
  static elapsedMillis runtime;

  if (_fx_active == fxMajorPeak) {
    if (runtime.toSeconds() >= intervalDefault()) {
      finished = true;
    }
  } else {
    runtime.reset();
    _fx_active = fxMajorPeak;
  }

  Color_t color_choice = Color::black();

  float mpeak, mag;

  _i2s->majorPeak(mpeak, mag);

  const float mag_low_freq = 15.0;
  const float mag_generic = 0.0;

  if (withinRange(mpeak, 20.0, 200.0, mag, mag_low_freq)) {
    color_choice = Color(mag, 0, 0, 0);

  } else if (withinRange(mpeak, 200.0, 300.0, mag, mag_low_freq)) {
    color_choice = Color(0, mag, 0, 0);

  } else if (withinRange(mpeak, 300.0, 500.0, mag, mag_generic)) {
    color_choice = Color(0, mag * 0.25, mag * 0.25, 0);

  } else if (withinRange(mpeak, 500.0, 1000.0, mag, mag_generic)) {
    color_choice = Color(0, mag * 0.5, mag * 0.5, 0);

  } else if (withinRange(mpeak, 1000.0, 1500.0, mag, mag_generic)) {
    color_choice = Color(0, 0, 0, mag);

  } else if (withinRange(mpeak, 1500.0, 2000.0, mag, mag_generic)) {
    color_choice = Color(0, 0, mag, mag);

  } else if (withinRange(mpeak, 2000, 17000, mag, mag_generic)) {
    color_choice = Color(mag * 0.75, mag * 0.45, 0, 0);
  }

  if (!finished) {

    _main->color(color_choice);
    _fill->color(color_choice);

    interval() = 0.022f;
  } else {
    FaderOpts_t fade_out{.origin = 0x00,
                         .dest = Color::black(),
                         .travel_secs = 0.2f,
                         .use_origin = false};

    _main->fadeTo(fade_out);
    _fill->fadeTo(fade_out);
  }

  return finished;
}

void LightDeskFx::primaryColorsCycle() {
  Fx_t fill_fx_map[13] = {fxNone};
  fill_fx_map[6] = fxWhiteFadeInOut;
  fill_fx_map[7] = fxFastStrobeSound;
  fill_fx_map[8] = fxRgbwGradientFast;

  _main->autoRun(fxPrimaryColorsCycle);

  const auto roll = roll2D6();
  const auto fill_fx = fill_fx_map[roll];

  if (fill_fx == fxNone) {
    _fill->dark();
  } else {
    _fill->autoRun(fill_fx);
  }

  if (fill_fx == fxNone) {
    intervalReduceTo(0.25f);
  } else {
    intervalReduceTo(0.35f);
  }
}

void LightDeskFx::simpleStrobe() {
  _main->color(Color::white(), 0.55);

  switch (roll1D6()) {
  case 1:
  case 2:
    _fill->color(Color::red(), 0.70);
    break;

  case 3:
  case 4:
    _fill->color(Color::green(), 0.70);
    break;

  case 5:
  case 6:
    _fill->color(Color::blue(), 0.70);
    break;
  }

  intervalReduceTo(0.37);
}

void LightDeskFx::soundFastStrobe() {
  // index 0 and 1 are unused
  static const float intervals[13] = {0.0f,  0.0f,  0.25f, 0.20f, 0.15f,
                                      0.15f, 0.13f, 0.13f, 0.13f, 0.13f,
                                      0.17f, 0.18f, 0.18f};

  _main->autoRun(fxFastStrobeSound);

  const auto roll = roll2D6();

  intervalReduceTo(intervals[roll]);

  FaderOpts fader{
      .dest = Color::black(), .travel_secs = 0.0f, .use_origin = true};

  switch (roll) {
  case 2:
    _fill->color(Color::red());
    break;

  case 3:
    _fill->color(Color(0, 0, 0, 32));
    break;

  case 4:
    _fill->color(Color(0, 0, 32, 0), 0.50f);

    break;

  case 5:
    _fill->color(Color(0, 32, 0, 0), 0.75f);
    break;

  case 6:
    _fill->dark();
    break;

  case 7:
    fader.dest = Color::randomize();
    fader.travel_secs = intervalPercent(intervals[roll]);
    _fill->fadeTo(fader);
    break;

  case 8:
    _fill->autoRun(fxFastStrobeSound);
    break;

  case 9:
    _fill->autoRun(fxRedBlueGradient);
    break;

  case 10:
    fader.dest = Color::randomize();
    fader.travel_secs = intervalPercent(intervals[roll]);
    _fill->fadeTo(fader);
    break;

  case 11:
    _fill->color(Color::blue());
    break;

  case 12:
    _fill->color(Color::green());
    break;
  }
}

void LightDeskFx::soundWashed() {
  const FaderOpts fo{.origin = Color::white(),
                     .dest = Color::black(),
                     .travel_secs = 3.1f,
                     .use_origin = true};

  _fill->fadeTo(fo);
  _main->autoRun(fxFastStrobeSound);

  intervalReduceTo(0.50f);
}

void LightDeskFx::whiteFadeInOut() {
  _main->autoRun(fxWhiteFadeInOut);

  auto roll = roll2D6();

  if (roll == 7) {
    _fill->color(Color::randomize());
    intervalReduceTo(0.15);
  } else if (roll == 6) {
    _fill->autoRun(fxFastStrobeSound);
    intervalReduceTo(0.20);
  } else {
    _fill->dark();
    intervalReduceTo(0.17);
  }
}

} // namespace lightdesk
} // namespace ruth
