/*
    lightdesk/lightdesk.cpp - Ruth Light Desk
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

#include "lightdesk/headunit.hpp"
#include "lightdesk/headunits/ac_power.hpp"
#include "lightdesk/headunits/discoball.hpp"
#include "lightdesk/headunits/elwire.hpp"
#include "lightdesk/headunits/ledforest.hpp"
#include "lightdesk/lightdesk.hpp"
#include "protocols/dmx.hpp"
#include "readings/text.hpp"

using std::make_shared;

namespace ruth {
namespace lightdesk {

LightDesk::LightDesk() {}

LightDesk::~LightDesk() {}

void LightDesk::init() {

  if (!_dmx) {
    _dmx = std::make_shared<Dmx>();
  }

  _dmx->start();
  _dmx->addHeadUnit(make_shared<AcPower>());
  _dmx->addHeadUnit(make_shared<DiscoBall>(1)); // pwm 1
  _dmx->addHeadUnit(make_shared<ElWire>(2));    // pwm 2
  _dmx->addHeadUnit(make_shared<ElWire>(3));    // pwm 3
  _dmx->addHeadUnit(make_shared<LedForest>(4)); // pwm 4
}

void preStart() {}

void LightDesk::start() { init(); }

void LightDesk::stop() { _dmx->stop(); }

// Static Declarations for LightDesk related classes
DRAM_ATTR bool PulseWidthHeadUnit::_timer_configured = false;
DRAM_ATTR const ledc_timer_config_t PulseWidthHeadUnit::_ledc_timer = {
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_13_BIT,
    .timer_num = LEDC_TIMER_1,
    .freq_hz = 5000,
    .clk_cfg = LEDC_AUTO_CLK};

} // namespace lightdesk
} // namespace ruth
