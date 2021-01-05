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

LightDeskFx::LightDeskFx(PinSpot_t *main, PinSpot_t *fill)
    : _main(main), _fill(fill){};

void LightDeskFx::execute(Fx_t next_fx) {
  _fx_next = next_fx;

  execute();
}

void LightDeskFx::basic(Fx_t fx) {
  _stats.basic++; // track we're rendering a basic fx

  intervalReset();
  _main->autoRun(fx); // always run the basic fx

  auto roll = dieRoll(); // roll a 1d6 (equal probabilies)

  switch (roll) {
  case 1:
    _fill->autoRun(fx); // run the same fx on fill
    break;

  case 2:
    _fill->dark();
    break;

  case 3:
    _fill->color(Color(64, 64, 64, 64), 0.75f); // dim white, 75% strobe
    break;

  case 4:
    _fill->autoRun(fxFastStrobeSound);
    break;

  case 5:
    _fill->autoRun(fxColorCycleSound);
    break;

  case 6:
    _fill->autoRun(fxRgbwGradientFast);
    break;
  }

  if (fx == _fx_prev) {
    // prevent extended periods of basic fx
    intervalChange(randomPercent(95, 99));
  } else {
    intervalChange(randomPercent(50, 75));
  }
}

bool LightDeskFx::execute(bool *finished_ptr) {
  bool finished = true;
  // bool default_handling = false;

  // quickly return if there isn't an active or next fx
  if ((_fx_next == fxNone) && (_fx_active == fxNone)) {
    _fx_interval = 1.0f; // just in case to prevent runaway task
    return finished;
  }

  switch (_fx_next) {
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

  default:
    basic(_fx_next);
    break;
  }

  // when finished set internal state to reflect no active fx
  if (finished) {
    _fx_prev = _fx_next;
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

void LightDeskFx::fullSpectrumCycle() {
  intervalReset();
  _main->autoRun(_fx_next);
  _fill->color(Color(128, 0, 0, 0), 0.75);
  intervalChange(randomPercent(75, 85));
}

void LightDeskFx::start() { execute(fxColorBars); }

//
// Fx Functions
//

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

void LightDeskFx::primaryColorsCycle() {
  Fx_t fill_fx_map[13] = {};
  fill_fx_map[6] = fxWhiteFadeInOut;
  fill_fx_map[7] = fxFastStrobeSound;
  fill_fx_map[8] = fxRgbwGradientFast;

  intervalReset();

  _main->autoRun(fxPrimaryColorsCycle);

  const auto roll = roll2D6();
  const auto fill_fx = fill_fx_map[roll];

  if (fill_fx == fxDark) {
    _fill->dark();
  } else {
    _fill->autoRun(fill_fx);
  }

  if (fill_fx == fxDark) {
    intervalChange(0.75);
  } else {
    intervalChange(0.44);
  }
}

void LightDeskFx::simpleStrobe() {
  intervalReset();

  _main->color(Color(0, 0, 0, 255), 0.40);
  _fill->color(Color(255, 0, 0, 0), 0.60);

  intervalChange(0.63);
}

void LightDeskFx::soundFastStrobe() {
  // index 0 and 1 are unused
  static const float interval_changes[13] = {0.0f,  0.0f,  0.44f, 0.49f,
                                             0.65f, 0.35f, 0.30f, 0.66f,
                                             0.70f, 0.50f, 0.53f, 0.48f};

  intervalReset();

  _main->autoRun(fxFastStrobeSound);

  const auto roll = diceRoll();

  intervalChange(interval_changes[roll]);

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
    _fill->color(Color(0, 32, 0, 0), 0.01f);
    break;

  case 6:
  case 8:
    _fill->autoRun(fxFastStrobeSound);
    break;

  case 7:
    fader.dest = Color::randomize();
    fader.travel_secs = intervalPercent(0.25f);
    _fill->fadeTo(fader);
    break;

  case 9:
    _fill->autoRun(fxRedBlueGradient);
    break;

  case 10:
    _fill->dark();
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
  intervalReset();

  const FaderOpts fo{.origin = Color::white(),
                     .dest = Color::black(),
                     .travel_secs = 3.1f,
                     .use_origin = true};

  _fill->fadeTo(fo);
  _main->autoRun(fxFastStrobeSound);

  intervalChange(0.65f);
}

void LightDeskFx::whiteFadeInOut() {
  intervalReset();
  _main->autoRun(fxWhiteFadeInOut);

  auto roll = diceRoll();

  if (roll == 7) {
    _fill->color(Color::randomize());
    intervalChange(0.50);
  } else if (roll == 6) {
    _fill->autoRun(fxFastStrobeSound);
    intervalChange(0.50);
  } else {
    _fill->dark();
    intervalChange(0.75);
  }
}

} // namespace lightdesk
} // namespace ruth
