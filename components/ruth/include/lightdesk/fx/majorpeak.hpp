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

#include <deque>

#include "lightdesk/fx/base.hpp"

namespace ruth {
namespace lightdesk {
namespace fx {

class MajorPeak : public FxBase {
public:
  MajorPeak() : FxBase(fxMajorPeak) { // selectFrequencyColors();

    // initialize static frequency to color mapping
    if (_freq_colors.size() == 0) {
      initializeFrequencyColors();
    }
  }

  struct FreqColor {
    struct {
      Freq_t low;
      Freq_t high;
    } freq;

    Color_t color;
  };

  typedef std::deque<FreqColor> FreqColorList_t;

protected:
  void executeEffect() {
    // PinSpot_t *spots[2] = {};
    //
    // if (_swap_spots) {
    //   spots[0] = pinSpotFill();
    //   spots[1] = pinSpotMain();
    // } else {
    //   spots[0] = pinSpotMain();
    //   spots[1] = pinSpotFill();
    // }

    // handle bass
    if (i2s()->bass()) {
      elWireDanceFloor()->pulse();
      elWireEntry()->pulse();
      ledForest()->pulse();
    }

    PeakInfo peak = i2s()->majorPeak();

    if (peak.dB > 0) {
      Color_t color = frequencyMapToColor(peak);

      if (color.notBlack()) {
        color.scale(peak.dB);

        FaderOpts_t freq_fade{.origin = color,
                              .dest = Color::black(),
                              .travel_secs = 0.6f,
                              .use_origin = true};

        if (peak.freq <= 180.0) {
          bool start_fade = true;

          if (pinSpotFill()->isFading()) {
            const FaderOpts &active_opts = pinSpotFill()->fadeCurrentOpts();

            if (active_opts.origin == freq_fade.origin) {
              start_fade = false;
            }
          }

          if (start_fade) {
            pinSpotFill()->fadeTo(freq_fade);
          }

          if (pinSpotMain()->isFading() == false) {
            pinSpotMain()->fadeTo(freq_fade);
          }
        }

        if (peak.freq > 180.0) {
          bool start_fade = true;
          freq_fade.travel_secs = 0.6;

          if (pinSpotMain()->isFading()) {
            const FaderOpts &active_opts = pinSpotMain()->fadeCurrentOpts();

            if (active_opts.origin == freq_fade.origin) {
              start_fade = false;
            }
          }

          if (start_fade) {
            pinSpotMain()->fadeTo(freq_fade);
          }

          if (pinSpotFill()->isFading() == false) {
            pinSpotFill()->fadeTo(freq_fade);
          }
        }
      }

      // if (start_fade) {
      //   spots[0]->fadeTo(freq_fade);
      //   _swap_spots = !_swap_spots;
      // }
    }
  }

private:
  Color_t frequencyMapToColor(PeakInfo &peak) {
    Color_t mapped_color;

    for (const FreqColor &colors : _freq_colors) {
      const Freq_t freq = peak.freq;

      if ((freq > colors.freq.low) && (freq <= colors.freq.high)) {
        mapped_color = colors.color;
      }

      if (mapped_color.notBlack()) {
        break;
      }
    }

    return mapped_color;
  }

  void initializeFrequencyColors() {
    _freq_colors.emplace_back(FreqColor{.freq = {.low = 29, .high = 60},
                                        .color = Color::fireBrick()});

    pushFrequencyColor(80, Color::crimson());
    pushFrequencyColor(120, Color::red());
    pushFrequencyColor(180, Color::deepPink());
    pushFrequencyColor(220, Color::dodgerBlue());
    pushFrequencyColor(280, Color::cyan());
    pushFrequencyColor(350, Color::powderBlue());
    pushFrequencyColor(490, Color::gold());
    pushFrequencyColor(550, Color::forestGreen());
    pushFrequencyColor(610, Color::lawnGreen());
    pushFrequencyColor(680, Color::green());
    pushFrequencyColor(750, Color::seaGreen());
    pushFrequencyColor(850, Color::blue());
    pushFrequencyColor(950, Color::teal());
    pushFrequencyColor(1050, Color::deepSkyBlue());
    pushFrequencyColor(1500, Color::cadetBlue());
    pushFrequencyColor(3000, Color::steelBlue());
    pushFrequencyColor(5000, Color::powderBlue());
    pushFrequencyColor(7000, Color::darkViolet());
    pushFrequencyColor(10000, Color::magenta());
    pushFrequencyColor(12000, Color::yellow());
    pushFrequencyColor(15000, Color::hotPink());
    pushFrequencyColor(22000, Color::bright());
  }

  void pushFrequencyColor(Freq_t high, const Color_t &color) {
    const FreqColor &last = _freq_colors.back();
    const FreqColor &next = FreqColor{
        .freq = {.low = last.freq.high, .high = high}, .color = color};

    _freq_colors.emplace_back(next);
  }

  // void selectFrequencyColors() {
  //   const auto roll = roll2D6();
  //
  //   switch (roll) {
  //
  //   case 5:
  //   case 6:
  //   case 7:
  //   case 8:
  //     _frequency_color_selected_idx = 0;
  //     break;
  //
  //   case 2:
  //   case 12:
  //     _frequency_color_selected_idx = 3;
  //     runtimeReduceTo(0.50);
  //     break;
  //
  //   case 3:
  //   case 4:
  //     _frequency_color_selected_idx = 1;
  //     break;
  //
  //   case 9:
  //   case 10:
  //     _frequency_color_selected_idx = 2;
  //     break;
  //   }
  // }

private:
  bool _swap_spots = false;

  const float _mid_range_frequencies[13] = {349.2, 370.0, 392.0, 415.3, 440.0,
                                            466.2, 493.9, 523.2, 544.4, 587.3,
                                            622.2, 659.3, 698.5};

  static FreqColorList_t _freq_colors;

}; // namespace fx

} // namespace fx
} // namespace lightdesk
} // namespace ruth

#endif
