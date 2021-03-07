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
      // initializeFrequencyColors();
      makePalette();
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
  typedef std::deque<FreqColor> Palette_t;

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
      Color_t color = lookupColor(peak);

      if (color.notBlack()) {
        color.scale(peak.dB);

        if (peak.freq <= 180.0) {
          handleLowFreq(peak, color);
        } else {
          handleOtherFreq(peak, color);
        }
      }
    }
    // } else {
    //   // if a PinSpot isn't busy (aka fading) then start it fading to
    //   // black to handle periods of silence
    //   if (pinSpotMain()->isFading() == false) {
    //     pinSpotMain()->fadeOut();
    //   }
    //
    //   if (pinSpotFill()->isFading() == false) {
    //     pinSpotFill()->fadeOut();
    //   }
    // }
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

  void handleLowFreq(PeakInfo &peak, const Color_t &color) {
    bool start_fade = true;

    FaderOpts_t freq_fade{.origin = color,
                          .dest = Color::black(),
                          .travel_secs = 0.7f,
                          .use_origin = true};

    const auto fading = pinSpotFill()->isFading();

    if (fading) {
      if ((_last_peak.fill.freq <= 180.0f) &&
          (_last_peak.fill.index == peak.index)) {
        start_fade = false;
      }
    }

    if (start_fade) {
      pinSpotFill()->fadeTo(freq_fade);
      _last_peak.fill = peak;
    } else if (!fading) {
      _last_peak.fill = Peak::zero();
    }
  }

  void handleOtherFreq(PeakInfo &peak, const Color_t &color) {
    bool start_fade = true;
    const FaderOpts_t main_fade{.origin = color,
                                .dest = Color::black(),
                                .travel_secs = 0.7f,
                                .use_origin = true};

    const auto main_fading = pinSpotMain()->isFading();
    const auto fill_fading = pinSpotFill()->isFading();

    if (main_fading && (_last_peak.main.index == peak.index)) {
      start_fade = false;
    }

    if (start_fade) {
      pinSpotMain()->fadeTo(main_fade);
      _last_peak.main = peak;
    } else if (!main_fading) {
      _last_peak.main = Peak::zero();
    }

    const FaderOpts_t alt_fade{.origin = color,
                               .dest = Color::black(),
                               .travel_secs = 0.7f,
                               .use_origin = true};

    if ((_last_peak.fill.dB < peak.dB) || !fill_fading) {
      pinSpotFill()->fadeTo(alt_fade);
      _last_peak.fill = peak;
    }
  }

  void initializeFrequencyColors() {
    _freq_colors.emplace_back(
        FreqColor{.freq = {.low = 29, .high = 60}, .color = Color::red()});

    // colors pushed with low = previous color high
    pushFrequencyColor(120, Color::fireBrick());
    pushFrequencyColor(160, Color::crimson());
    pushFrequencyColor(180, Color::blue());
    pushFrequencyColor(240, Color::yellow25());
    pushFrequencyColor(360, Color::yellow50());
    pushFrequencyColor(380, Color::yellow());
    pushFrequencyColor(320, Color::yellow75());
    pushFrequencyColor(350, Color::steelBlue());
    pushFrequencyColor(490, Color::green());
    pushFrequencyColor(550, Color::gold());
    pushFrequencyColor(610, Color::limeGreen());
    pushFrequencyColor(680, Color::cadetBlue());
    pushFrequencyColor(750, Color::seaGreen());
    pushFrequencyColor(850, Color::deepPink());
    pushFrequencyColor(950, Color::blueViolet());
    pushFrequencyColor(1050, Color::deepSkyBlue());
    pushFrequencyColor(1500, Color::pink());
    pushFrequencyColor(3000, Color::steelBlue());
    pushFrequencyColor(5000, Color::hotPink());
    pushFrequencyColor(7000, Color::darkViolet());
    pushFrequencyColor(10000, Color::magenta());
    pushFrequencyColor(12000, Color::deepSkyBlue());
    pushFrequencyColor(15000, Color::darkViolet());
    pushFrequencyColor(22000, Color::bright());
  }

  Color_t lookupColor(PeakInfo &peak) {
    Color_t mapped_color;

    for (const FreqColor &colors : _palette) {
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

  void makePalette() {
    const FreqColor first_color =
        FreqColor{.freq = {.low = 10, .high = 60}, .color = Color::red()};

    _palette.emplace_back(first_color);

    pushPaletteColor(120, Color::fireBrick());
    pushPaletteColor(160, Color::crimson());
    pushPaletteColor(180, Color(44, 21, 119));
    pushPaletteColor(260, Color::blue());
    pushPaletteColor(300, Color::yellow75());
    pushPaletteColor(320, Color::gold());
    pushPaletteColor(350, Color::yellow());
    pushPaletteColor(390, Color(94, 116, 140)); // slate blue
    pushPaletteColor(490, Color::green());
    pushPaletteColor(550, Color(224, 155, 0)); // light orange
    pushPaletteColor(610, Color::limeGreen());
    pushPaletteColor(710, Color::seaGreen());
    pushPaletteColor(850, Color::deepPink());
    pushPaletteColor(950, Color::blueViolet());
    pushPaletteColor(1050, Color::magenta());
    pushPaletteColor(1500, Color::pink());
    pushPaletteColor(3000, Color::steelBlue());
    pushPaletteColor(5000, Color::hotPink());
    pushPaletteColor(7000, Color::darkViolet());
    pushPaletteColor(10000, Color(245, 242, 234));
    pushPaletteColor(12000, Color(245, 243, 215));
    pushPaletteColor(15000, Color(228, 228, 218));
    pushPaletteColor(22000, Color::bright());
  }

  void pushFrequencyColor(Freq_t high, const Color_t &color) {
    const FreqColor &last = _freq_colors.back();
    const FreqColor &next = FreqColor{
        .freq = {.low = last.freq.high, .high = high}, .color = color};

    _freq_colors.emplace_back(next);
  }

  void pushPaletteColor(Freq_t high, const Color_t &color) {
    const FreqColor &last = _palette.back();
    const FreqColor &next = FreqColor{
        .freq = {.low = last.freq.high, .high = high}, .color = color};

    _palette.emplace_back(next);
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
  static Palette_t _palette;

  struct {
    Peak_t main = Peak::zero();
    Peak_t fill = Peak::zero();
  } _last_peak;
};

} // namespace fx
} // namespace lightdesk
} // namespace ruth

#endif
