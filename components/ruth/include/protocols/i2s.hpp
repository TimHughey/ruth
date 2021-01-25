/*
    protocols/i2s.hpp - Ruth I2S
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

#ifndef _ruth_i2s_hpp
#define _ruth_i2s_hpp

#include <algorithm>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <driver/i2s.h>

#include <lwip/err.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>

#include "ArduinoFFT/arduinoFFT.h"
#include "lightdesk/types.hpp"
#include "local/types.hpp"
#include "misc/elapsed.hpp"

namespace ruth {

typedef class I2s I2s_t;

class I2s {
  using I2sStats_t = lightdesk::I2sStats_t;

public:
  I2s();
  ~I2s();

  float majorPeak(float &mpeak, float &mag);

  void printFrequencyBins() {
    printf("FFT Frequency Bins\n");
    printf("------------------\n");

    printf("Frequency Bins: %.1f  Frequency Interval: %.1f\n\n",
           _freq_bin_count, _freq_bin_interval);

    uint16_t row_item = 0;
    for (auto bin = 0; bin < _freq_bin_count; bin++) {
      float low, high;

      if ((row_item++) == 0) {
        printf("  ");
      }

      binToFrequency(bin, low, high);
      printf("%3d %8.1f ", bin, low);

      if (row_item > 8) {
        printf("\n");
        row_item = 0;
      }
    }

    printf("\n\n");
  }

  void samplePrint() { taskNotify(NotifySamplePrint); }
  void sampleStopPrint() { taskNotify(NotifySampleStopPrint); }

  void setPrintSeconds(uint32_t secs) { _print_ms = secs * 1000; }

  void start() { taskStart(); }
  void stop() { taskNotify(NotifyStop); }

private:
  typedef enum { INIT = 0x00, PROCESS_AUDIO, STOP, SHUTDOWN } I2sMode_t;

private:
  void binToFrequency(uint16_t bin, float &freq_lowside,
                      float &freq_highside) const {
    const float lowside = _freq_bin_interval * bin;
    const float highside = lowside + (_freq_bin_interval - 0.09f);

    freq_lowside = lowside;
    freq_highside = highside;
  }

  void compute(uint8_t *buffer, size_t len);

  void fft(uint8_t *buffer, size_t len);
  inline void fftRecordElapsed(elapsedMicros &e) {
    e.freeze();

    const size_t slots = sizeof(_stats.durations.fft_us) / sizeof(uint32_t);
    if (_stats.temp.fft_us_idx >= slots) {
      _stats.temp.fft_us_idx = 0;
    }

    size_t &slot = _stats.temp.fft_us_idx;

    _stats.durations.fft_us[slot++] = (uint32_t)e;
  }

  void filterNoise();

  bool handleNotifications();

  bool samplesRx();
  inline void samplesRxElapsed(elapsedMicros &e) {
    e.freeze();

    const size_t slots = sizeof(_stats.durations.rx_us) / sizeof(uint32_t);
    if (_stats.temp.rx_us_idx >= slots) {
      _stats.temp.rx_us_idx = 0;
    }

    size_t &slot = _stats.temp.rx_us_idx;

    _stats.durations.rx_us[slot++] = (uint32_t)e;
  }
  bool samplesUdpTx();

  inline bool silence() const { return _noise; }

  inline TaskHandle_t task() const { return _task.handle; }
  static void taskCore(void *task_instance);
  void taskInit();
  void taskLoop();
  BaseType_t taskNotify(NotifyVal_t nval);
  void taskStart();

  void trackValMinMax(const int32_t left_val, const int32_t right_val) {
    const int32_t vmin = (left_val < right_val) ? left_val : right_val;
    const int32_t vmax = (left_val > right_val) ? left_val : right_val;

    int32_t &min = _stats.raw_val.min24;
    int32_t &max = _stats.raw_val.max24;

    if (vmin < min) {
      min = vmin;
    }

    if (vmax > max) {
      max = vmax;
    }
  }

  bool udpInit();
  void udpSend(uint8_t *data, size_t len);

private:
  I2sMode_t _mode = INIT;
  esp_err_t _init_rc = ESP_OK;

  int _bit_shift = 8;
  bool _sample_print = false;
  uint32_t _print_ms = 15 * 1000;
  elapsedMillis _print_elapsed;

  const i2s_port_t _i2s_port = I2S_NUM_0;
  const int _dma_buff = 1024;
  const size_t _data_len = 4;

  uint32_t _eq_max_depth = 4;

  uint8_t *_raw = nullptr;

  I2sStats_t _stats;

  static const size_t _sample_rate = 44100;
  static const size_t _vsamples = 1024;
  static const size_t _vsamples_chan = _vsamples / 2;
  float _vreal_left[_vsamples_chan] = {};
  float _vreal_right[_vsamples_chan] = {};
  float _vimag[_vsamples_chan] = {};
  float _wfactors[_vsamples_chan] = {};
  const float _freq_bin_count = _vsamples_chan / 2.0;
  const float _freq_bin_interval = _sample_rate / _freq_bin_count;

  float _noise_filters[4][3] = {
      // {low_freq, high_freq, mag}
      {20.0, 58.0, 10.0},
      {58.0, 65.0, 9.0},
      {65.0, 150.0, 9.0},
      {150.0, 21000.0, 9.0}};

  float _mpeak = 0.0;
  float _mpeak_mag = 0.0;
  bool _noise = true;

  ArduinoFFT<float> _fft = ArduinoFFT<float>(
      _vreal_left, _vimag, _vsamples_chan, _sample_rate, _wfactors);

  uint8_t _cols = 20;
  uint8_t _row_items = 0;

  // UDP client
  const int _addr_family = AF_INET;
  const int _ip_protocol = IPPROTO_IP;
  const char *_host_ip = "192.168.2.53";
  const int _port_raw = 44100;
  const int _port_text = 44101;

  struct sockaddr_in _dest_raw = {};
  struct sockaddr_in _dest_text = {};
  int _socket_raw = -1;
  int _socket_text = -1;
  uint32_t _udp_errors = 0;

  Task_t _task = {
      .handle = nullptr, .data = nullptr, .priority = 19, .stackSize = 4096};
};
} // namespace ruth

#endif