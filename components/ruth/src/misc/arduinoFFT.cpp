#include <limits>

#include <local/types.hpp>

#include "external/arduinoFFT.hpp"

const DRAM_ATTR float ArduinoFFT::_WindowCompensationFactors[10] = {
    1.0000000000 * 2.0, // rectangle (Box car)
    1.8549343278 * 2.0, // hamming
    1.8554726898 * 2.0, // hann
    2.0039186079 * 2.0, // triangle (Bartlett)
    2.8163172034 * 2.0, // nuttall
    2.3673474360 * 2.0, // blackman
    2.7557840395 * 2.0, // blackman nuttall
    2.7929062517 * 2.0, // blackman harris
    3.5659039231 * 2.0, // flat top
    1.5029392863 * 2.0  // welch
};

void IRAM_ATTR ArduinoFFT::complexToMagnitude() const {
  // vM is half the size of vReal and vImag
  for (uint_fast16_t i = 0; i < _samples; i++) {
    _vReal[i] = sqrt(sq(_vReal[i]) + sq(_vImag[i]));
  }
}

void IRAM_ATTR ArduinoFFT::compute(FFTDirection dir) const {
  // Reverse bits /
  uint_fast16_t j = 0;
  for (uint_fast16_t i = 0; i < (_samples - 1); i++) {
    if (i < j) {
      swap(_vReal[i], _vReal[j]);
      if (dir == FFTDirection::Reverse) {
        swap(_vImag[i], _vImag[j]);
      }
    }
    uint_fast16_t k = (_samples >> 1);
    while (k <= j) {
      j -= k;
      k >>= 1;
    }
    j += k;
  }

  // Compute the FFT
  float c1 = -1.0;
  float c2 = 0.0;
  uint_fast16_t l2 = 1;
  for (uint_fast8_t l = 0; (l < _power); l++) {
    uint_fast16_t l1 = l2;
    l2 <<= 1;
    float u1 = 1.0;
    float u2 = 0.0;
    for (j = 0; j < l1; j++) {
      for (uint_fast16_t i = j; i < _samples; i += l2) {
        uint_fast16_t i1 = i + l1;
        float t1 = u1 * _vReal[i1] - u2 * _vImag[i1];
        float t2 = u1 * _vImag[i1] + u2 * _vReal[i1];
        _vReal[i1] = _vReal[i] - t1;
        _vImag[i1] = _vImag[i] - t2;
        _vReal[i] += t1;
        _vImag[i] += t2;
      }
      float z = ((u1 * c1) - (u2 * c2));
      u2 = ((u1 * c2) + (u2 * c1));
      u1 = z;
    }

    float cTemp = 0.5 * c1;
    c2 = sqrt(0.5 - cTemp);
    c1 = sqrt(0.5 + cTemp);

    c2 = dir == FFTDirection::Forward ? -c2 : c2;
  }
  // Scaling for reverse transform
  if (dir != FFTDirection::Forward) {
    for (uint_fast16_t i = 0; i < _samples; i++) {
      _vReal[i] /= _samples;
      _vImag[i] /= _samples;
    }
  }
}

void IRAM_ATTR ArduinoFFT::dcRemoval(const float mean) const {
  for (uint_fast16_t i = 1; i < ((_samples >> 1) + 1); i++) {
    _vReal[i] -= mean;
  }
}

void IRAM_ATTR ArduinoFFT::findPeaks() {
  _peaks.clear();

  for (uint_fast16_t i = 1; i < ((_samples >> 1) + 1); i++) {
    const float a = _vReal[i - 1];
    const float b = _vReal[i];
    const float c = _vReal[i + 1];

    if ((a < b) && (b > c)) {

      // once
      if (_peaks.size() == (_peaks.capacity() - 1)) {
        break;
      }

      float freq = freqAtIndex(i);
      float db = dbAtIndex(i);

      if (db > 20.0f) {
        const Peak_t tuple = {.index = i, .freq = freq, .dB = db};
        _peaks.push_back(tuple);
      }
    }
  }

  std::sort(_peaks.begin(), _peaks.end(), Peak_t::higherdB);
}

void IRAM_ATTR ArduinoFFT::windowing(FFTWindow windowType, FFTDirection dir,
                                     bool withCompensation) {
  // check if values are already pre-computed for the correct window type and
  // compensation
  if (_windowWeighingFactors && _weighingFactorsComputed &&
      _weighingFactorsFFTWindow == windowType &&
      _weighingFactorsWithCompensation == withCompensation) {
    // yes. values are precomputed
    if (dir == FFTDirection::Forward) {
      for (uint_fast16_t i = 0; i < (_samples >> 1); i++) {
        _vReal[i] *= _windowWeighingFactors[i];
        _vReal[_samples - (i + 1)] *= _windowWeighingFactors[i];
      }
    } else {
      for (uint_fast16_t i = 0; i < (_samples >> 1); i++) {
        _vReal[i] /= _windowWeighingFactors[i];
        _vReal[_samples - (i + 1)] /= _windowWeighingFactors[i];
      }
    }
  } else {
    // no. values need to be pre-computed or applied
    float samplesMinusOne = (float(_samples) - 1.0);
    float compensationFactor =
        _WindowCompensationFactors[static_cast<uint_fast8_t>(windowType)];
    for (uint_fast16_t i = 0; i < (_samples >> 1); i++) {
      float indexMinusOne = float(i);
      float ratio = (indexMinusOne / samplesMinusOne);
      float weighingFactor = 1.0;
      // Compute and record weighting factor
      switch (windowType) {
      case FFTWindow::Rectangle: // rectangle (box car)
        weighingFactor = 1.0;
        break;
      case FFTWindow::Hamming: // hamming
        weighingFactor = 0.54 - (0.46 * cos(TWO_PI * ratio));
        break;
      case FFTWindow::Hann: // hann
        weighingFactor = 0.54 * (1.0 - cos(TWO_PI * ratio));
        break;
      case FFTWindow::Triangle: // triangle (Bartlett)
        weighingFactor =
            1.0 - ((2.0 * abs(indexMinusOne - (samplesMinusOne / 2.0))) /
                   samplesMinusOne);
        break;
      case FFTWindow::Nuttall: // nuttall
        weighingFactor = 0.355768 - (0.487396 * (cos(TWO_PI * ratio))) +
                         (0.144232 * (cos(FOUR_PI * ratio))) -
                         (0.012604 * (cos(SIX_PI * ratio)));
        break;
      case FFTWindow::Blackman: // blackman
        weighingFactor = 0.42323 - (0.49755 * (cos(TWO_PI * ratio))) +
                         (0.07922 * (cos(FOUR_PI * ratio)));
        break;
      case FFTWindow::Blackman_Nuttall: // blackman nuttall
        weighingFactor = 0.3635819 - (0.4891775 * (cos(TWO_PI * ratio))) +
                         (0.1365995 * (cos(FOUR_PI * ratio))) -
                         (0.0106411 * (cos(SIX_PI * ratio)));
        break;
      case FFTWindow::Blackman_Harris: // blackman harris
        weighingFactor = 0.35875 - (0.48829 * (cos(TWO_PI * ratio))) +
                         (0.14128 * (cos(FOUR_PI * ratio))) -
                         (0.01168 * (cos(SIX_PI * ratio)));
        break;
      case FFTWindow::Flat_top: // flat top
        weighingFactor = 0.2810639 - (0.5208972 * cos(TWO_PI * ratio)) +
                         (0.1980399 * cos(FOUR_PI * ratio));
        break;
      case FFTWindow::Welch: // welch
        weighingFactor = 1.0 - (sq(indexMinusOne - samplesMinusOne / 2.0) /
                                (samplesMinusOne / 2.0));
        break;
      }
      if (withCompensation) {
        weighingFactor *= compensationFactor;
      }
      if (_windowWeighingFactors) {
        _windowWeighingFactors[i] = weighingFactor;
      }
      if (dir == FFTDirection::Forward) {
        _vReal[i] *= weighingFactor;
        _vReal[_samples - (i + 1)] *= weighingFactor;
      } else {
        _vReal[i] /= weighingFactor;
        _vReal[_samples - (i + 1)] /= weighingFactor;
      }
    }
    // mark cached values as pre-computed
    _weighingFactorsFFTWindow = windowType;
    _weighingFactorsWithCompensation = withCompensation;
    _weighingFactorsComputed = true;
  }
}
