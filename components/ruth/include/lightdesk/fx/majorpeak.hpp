/*
    lightdesk/fx/majorpeak.hpp -- LightDesk Effect Major Peak
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

#ifndef _ruth_lightdesk_fx_majorpeak_hpp
#define _ruth_lightdesk_fx_majorpeak_hpp

#include "lightdesk/fx/base.hpp"

namespace ruth {
namespace lightdesk {
namespace fx {

class MajorPeak : public FxBase {
public:
  MajorPeak() : FxBase(fxMajorPeak) { selectFrequencyColors(); }

protected:
  void executeEffect() {
    PinSpot_t *spots[2] = {pinSpotMain(), pinSpotFill()};

    if (_swap_spots) {
      spots[0] = pinSpotFill();
      spots[1] = pinSpotMain();
    }

    // handle bass
    if (i2s()->bass()) {
      elWireDanceFloor()->pulse();
      elWireEntry()->pulse();
      ledForest()->pulse();
    }

    PeakInfo peak = i2s()->majorPeak();

    if (peak.dB > 0) {
      size_t color_index;
      auto freq_known = frequencyKnown(peak.freq, color_index);

      if (freq_known) {
        Color freq_color = frequencyMapToColor(color_index);
        freq_color.scale(peak.dB);

        FaderOpts_t freq_fade{.origin = freq_color,
                              .dest = Color::black(),
                              .travel_secs = .6,
                              .use_origin = true};

        spots[0]->fadeTo(freq_fade);
        _swap_spots = !_swap_spots;
      }
    }

    return;
  }

private:
  bool frequencyKnown(float frequency, size_t &index) {
    bool rc = false;
    index = 0;

    for (auto k = 0; k < _frequency_count; k++) {
      const float low = _frequencies[k][0];
      const float high = _frequencies[k][1];

      if ((frequency > low) && (frequency <= high)) {
        rc = true;
        index = k;
        break;
      }
    }

    return rc;
  }

  Color_t frequencyMapToColor(size_t index) {
    Color_t color;

    if (index < _frequency_count) {
      color = _frequency_colors[_frequency_color_selected_idx][index];
    }

    return color;
  }

  void selectFrequencyColors() {
    const auto roll = roll2D6();

    switch (roll) {

    case 5:
    case 6:
    case 7:
    case 8:
      _frequency_color_selected_idx = 0;
      break;

    case 2:
    case 12:
      _frequency_color_selected_idx = 3;
      runtimeReduceTo(0.50);
      break;

    case 3:
    case 4:
      _frequency_color_selected_idx = 1;
      break;

    case 9:
    case 10:
      _frequency_color_selected_idx = 2;
      break;
    }
  }

private:
  bool _swap_spots = false;

  const float _mid_range_frequencies[13] = {349.2, 370.0, 392.0, 415.3, 440.0,
                                            466.2, 493.9, 523.2, 544.4, 587.3,
                                            622.2, 659.3, 698.5};
  static constexpr size_t _frequency_count = 14;
  size_t _frequency_color_selected_idx = 0;

  const float _frequencies[_frequency_count][2] = {
      {70.0, 170},        // 00: low bass
      {170.0, 200.0},     // 01: high bass
      {200.0, 250.0},     // 02
      {250.0, 300.0},     // 03
      {300.0, 350.0},     // 04
      {350.0, 400.0},     // 05
      {400.0, 550.0},     // 06
      {550.0, 900.0},     // 07
      {900.0, 1000.0},    // 08
      {1000.0, 2000.0},   // 09
      {2000.0, 3000.0},   // 10
      {3000.0, 5000.0},   // 11
      {5000.0, 8000.0},   // 12
      {8000.0, 22000.0}}; // 13}

  // const static DRAM_ATTR Color_t _frequencyColors[] = {
  //     Color(0, 0, 255, 0),   Color(0, 255, 0, 0),  Color(102, 255, 0, 0),
  //     Color(0, 255, 102, 0), Color(255, 99, 0, 0), Color(255, 236, 0, 0),
  //     Color(153, 255, 0, 0), Color(40, 255, 0, 0), Color(0, 255, 232, 0),
  //     Color(0, 124, 253, 0), Color(5, 0, 255, 0),  Color(69, 0, 234, 0),
  //     Color(87, 0, 158, 0)};

  const Color_t _frequency_colors[4][_frequency_count] = {
      {Color(128, 0, 0, 0), Color::red(), Color::green(), Color::blue(),
       Color::violet(), Color::yellow(), Color::amber(), Color::lightBlue(),
       Color::lightGreen(), Color::lightRed(), Color::lightViolet(),
       Color::lightYellow(), Color::bright()},

      {Color(0, 128, 0, 0), Color::green(), Color::blue(), Color::red(),
       Color::yellow(), Color::violet(), Color::lightBlue(), Color::amber(),
       Color::lightRed(), Color::lightGreen(), Color::lightYellow(),
       Color::lightViolet(), Color::bright()},

      {Color(0, 0, 128, 0), Color::blue(), Color::red(), Color::green(),
       Color::lightBlue(), Color::violet(), Color::yellow(), Color::amber(),
       Color::lightYellow(), Color::lightGreen(), Color::lightRed(),
       Color::lightViolet(), Color::bright()},

      {Color(0, 0, 0, 128), Color::white(), Color::bright(), Color::bright(),
       Color::bright(), Color::bright(), Color::bright(), Color::bright(),
       Color::bright(), Color::bright(), Color::bright(), Color::bright(),
       Color::bright()}

  };
};

} // namespace fx
} // namespace lightdesk
} // namespace ruth

#endif
