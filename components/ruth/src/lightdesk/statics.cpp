/*
    lightdesk/statics.cpp - Ruth LightDesk Statics Definition
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

#include "lightdesk/fx/majorpeak.hpp"
#include "lightdesk/headunits/pinspot/base.hpp"
#include "lightdesk/headunits/pinspot/color.hpp"
#include "lightdesk/headunits/pwm.hpp"

namespace ruth {
namespace lightdesk {

DRAM_ATTR float Color::_scale_min = 0;
DRAM_ATTR float Color::_scale_max = 0;

DRAM_ATTR bool PulseWidthHeadUnit::_timer_configured = false;
DRAM_ATTR const ledc_timer_config_t PulseWidthHeadUnit::_ledc_timer = {
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_13_BIT,
    .timer_num = LEDC_TIMER_1,
    .freq_hz = 5000,
    .clk_cfg = LEDC_AUTO_CLK};

namespace fx {
DRAM_ATTR FxConfig_t FxBase::_cfg;
DRAM_ATTR FxStats_t FxBase::_stats;

DRAM_ATTR MajorPeak::FreqColorList_t MajorPeak::_freq_colors = {};
DRAM_ATTR MajorPeak::Palette_t MajorPeak::_palette = {};

} // namespace fx
} // namespace lightdesk
} // namespace ruth
