/*

        FFT libray
        Copyright (C) 2010 Didier Longueville
        Copyright (C) 2014 Enrique Condes
        Copyright (C) 2020 Bim Overbohm (header-only, template, speed
   improvements)

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

*/

#ifndef ArduinoFFT_h /* Prevent loading library twice */
#define ArduinoFFT_h

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "misc/maverage.hpp"

enum class FFTDirection { Reverse, Forward };

enum class FFTWindow {
  Rectangle,        // rectangle (Box car)
  Hamming,          // hamming
  Hann,             // hann
  Triangle,         // triangle (Bartlett)
  Nuttall,          // nuttall
  Blackman,         // blackman
  Blackman_Nuttall, // blackman nuttall
  Blackman_Harris,  // blackman harris
  Flat_top,         // flat top
  Welch             // welch
};

// template <typename T> class ArduinoFFT {
class ArduinoFFT {
public:
  // Constructor
  ArduinoFFT(float *vReal, float *vImag, uint_fast16_t samples,
             float samplingFrequency, float *windowWeighingFactors = nullptr)
      : _vReal(vReal), _vImag(vImag), _samples(samples),
        _samplingFrequency(samplingFrequency),
        _windowWeighingFactors(windowWeighingFactors) {
    // Calculates the base 2 logarithm of sample count
    _power = 0;
    while (((samples >> _power) & 1) != 1) {
      _power++;
    }
  }

  // Destructor
  ~ArduinoFFT() {}

  inline float binFrequency(size_t y) {
    float frequency, magnitude;

    binFrequency(y, frequency, magnitude);

    return frequency;
  }

  inline void binFrequency(size_t y, float &frequency, float &magnitude) {
    freqAndValueAtY(y, frequency, magnitude);
  }

  void calculateComplexity();

  // Computes in-place complex-to-complex FFT
  void compute(FFTDirection dir) const;
  inline float complexity() const { return _complexity; }
  inline float complexityAvg() const { return _complexity_mavg.latest(); }
  inline float complexityFloor() const { return _complexity_floor; }
  inline void complexityFloor(const float floor) { _complexity_floor = floor; }

  void complexToMagnitude() const;

  inline float dbAtIndex(const size_t i) const {
    const float a = _vReal[i - 1];
    const float b = _vReal[i];
    const float c = _vReal[i + 1];

    const float db = 10 * log10(abs(a - (2.0 * b) + c));
    return db;
  }

  void dcRemoval() const;

  inline void freqAndValueAtY(size_t y, float &frequency,
                              float &magnitude) const {
    const float a = _vReal[y - 1];
    const float b = _vReal[y];
    const float c = _vReal[y + 1];

    float delta = 0.5 * ((a - c) / (a - (2.0 * b) + c));
    frequency = ((y + delta) * _samplingFrequency) / (_samples - 1);
    if (y == (_samples >> 1)) {
      // To improve calculation on edge values
      frequency = ((y + delta) * _samplingFrequency) / _samples;
    }
    // returned value: interpolated frequency peak apex
    // constexpr float fourth = 1.0 / 4.0;
    // magnitude = pow(abs(a - (2.0 * b) + c), fourth);
    magnitude = 10 * log10(abs(a - (2.0 * b) + c));
  }

  void windowing(FFTWindow windowType, FFTDirection dir,
                 bool withCompensation = false);

  inline float majorPeak() const {
    float frequency;
    float value;

    majorPeak(frequency, value);

    return frequency;
  }

  inline void majorPeak(float &frequency, float &value) const {
    float maxY = 0;
    uint_fast16_t IndexOfMaxY = 0;
    // If sampling_frequency = 2 * max_frequency in signal,
    // value would be stored at position samples/2
    for (uint_fast16_t i = 1; i < ((_samples >> 1) + 1); i++) {
      const float a = _vReal[i - 1];
      const float b = _vReal[i];
      const float c = _vReal[i + 1];

      if ((a < b) && (b > c)) {
        if (b > maxY) {
          maxY = b;
          IndexOfMaxY = i;
        }
      }
    }

    freqAndValueAtY(IndexOfMaxY, frequency, value);
  }

  void minimumReal(float &min) const;

  inline void process(float *vreal, float *vimag, float &mpeak,
                      float &mpeak_mag) {
    constexpr uint32_t one_sec_us = 1000 * 1000;
    if (_per_second >= one_sec_us) {
      _pps = _pp;
      _pp = 0;
      _per_second.reset();
    } else {
      _pp++;
    }

    setArrays(vreal, vimag);
    dcRemoval();
    windowing(FFTWindow::Hamming, FFTDirection::Forward);
    compute(FFTDirection::Forward);
    complexToMagnitude();
    majorPeak(mpeak, mpeak_mag);
    calculateComplexity();
  }

  inline uint_fast16_t processPerSecond() { return _pps; }

  // Get library revision
  static uint8_t revision() { return 0x19; }

  // Replace the data array pointers
  inline void setArrays(float *vReal, float *vImag) {
    _vReal = vReal;
    _vImag = vImag;
  }

private:
  static const float _WindowCompensationFactors[10];

  // Mathematial constants
#ifndef TWO_PI
  static constexpr float TWO_PI =
      6.28318531; // might already be defined in Arduino.h
#endif
  static constexpr float FOUR_PI = 12.56637061;
  static constexpr float SIX_PI = 18.84955593;

  static inline void Swap(float &x, float &y) {
    float temp = x;
    x = y;
    y = temp;
  }

  inline float sq(const float x) const { return x * x; }

  /* Variables */
  float *_vReal = nullptr;
  float *_vImag = nullptr;
  uint_fast16_t _samples = 0;
  float _samplingFrequency = 0;
  float *_windowWeighingFactors = nullptr;
  FFTWindow _weighingFactorsFFTWindow;
  bool _weighingFactorsWithCompensation = false;
  bool _weighingFactorsComputed = false;
  uint_fast8_t _power = 0;

  float _mean_mag = 0;
  float _mpeak = 0;
  float _mpeak_mag = 0;

  float _complexity_floor = 55.0;
  float _complexity = 0;
  ruth::MovingAverage<float, 7> _complexity_mavg;

  ruth::elapsedMicros _per_second;
  uint_fast16_t _pp = 0;
  uint_fast16_t _pps = 0;
};

#endif
