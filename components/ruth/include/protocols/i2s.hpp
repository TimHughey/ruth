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

#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <driver/i2s.h>
#include <esp_timer.h>

#include <lwip/err.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>

#include "external/arduinoFFT.hpp"
#include "lightdesk/types.hpp"
#include "local/types.hpp"
#include "misc/derivative.hpp"
#include "misc/elapsed.hpp"

namespace ruth {

typedef class I2s I2s_t;

using std::vector;

class I2s {
  using I2sStats_t = lightdesk::I2sStats_t;

public:
  I2s();
  ~I2s();

  inline bool bass(float &mag) const {
    mag = _bass_mag;

    return _bass;
  }

  float bassMagnitudeFloor() const { return _bass_mag_floor; }

  void bassMagnitudeFloor(const float floor) { _bass_mag_floor = floor; }

  float complexity() const { return _fft.complexity(); }
  float complexityAvg() const { return _fft.complexityAvg(); }

  inline void magnitudeMinMax(float &min, float &max) {
    min = _stats.magnitude.min;
    max = _stats.magnitude.max;
  }

  inline bool magnitudeRateOfChange(const float floor, float &roc, float &mag,
                                    float &freq) {
    bool rc = false;
    roc = _mag_history.rateOfChange(mag, freq);

    if ((roc > floor) || (roc < (floor * -1.0))) {
      rc = true;
    }

    return rc;
  }

  float magFloor() const { return _mag_floor; }
  void magFloor(const float floor) { _mag_floor = floor; }

  // inline float meanMagnitude() { return _fft.meanMagnitude(); }

  inline float majorPeak(float &mpeak, float &mag) {
    mpeak = 0;
    mag = 0;

    if (_mpeak_mag >= _mag_floor) {
      mpeak = _mpeak;
      mag = _mpeak_mag;
    }

    return mpeak;
  }

  // stats
  void stats(I2sStats_t &stats) {
    _stats.bass_mag_floor = _bass_mag_floor;

    stats = _stats;
  }

  void start() { taskStart(); }
  void stop() { taskNotify(NotifyStop); }

private:
  typedef enum { INIT = 0x00, PROCESS_AUDIO, STOP, SHUTDOWN } I2sMode_t;

private:
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
  inline void samplesRxElapsed(elapsedMicros &e, const size_t bytes_read) {
    e.freeze();

    const size_t slots = sizeof(_stats.durations.rx_us) / sizeof(uint32_t);
    if (_stats.temp.rx_us_idx >= slots) {
      _stats.temp.rx_us_idx = 0;
    }

    size_t &slot = _stats.temp.rx_us_idx;

    _stats.durations.rx_us[slot++] = (uint32_t)e;

    _stats.temp.byte_count += bytes_read;
  }
  bool samplesUdpTx();

  inline bool silence() const { return _noise; }

  void statsCalculate();
  static void statsCalculateCallback(void *data) {
    if (data) {
      I2s_t *i2s = (I2s_t *)data;

      i2s->taskNotify(NotifyStatsCalculate);
    }
  }

  inline TaskHandle_t task() const { return _task.handle; }
  static void taskCore(void *task_instance);
  void taskInit();
  void taskLoop();
  BaseType_t taskNotify(NotifyVal_t nval);
  void taskStart();

  void trackMagMinMax(const float mag) {
    if ((_stats.magnitude.window) > 2300) {
      _stats.magnitude.min = 300000.0;
      _stats.magnitude.max = 0.0;
      _stats.magnitude.window.reset();
    }

    if (mag > _stats.magnitude.max) {
      _stats.magnitude.max = mag;
    }

    if (mag < _stats.magnitude.min) {
      _stats.magnitude.min = mag;
    }
  }

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
  void udpPrint();
  void udpSend(uint8_t *data, size_t len);

private:
  I2sMode_t _mode = INIT;
  esp_err_t _init_rc = ESP_OK;

  static constexpr int _bit_shift = 8;
  TextBuffer<1000> _print_buffer;

  static constexpr i2s_port_t _i2s_port = I2S_NUM_0;
  static constexpr int _dma_buff = 1024;
  static constexpr uint32_t _eq_max_depth = 4;
  bool _need_driver_install = true;

  uint8_t *_raw = nullptr;

  I2sStats_t _stats;

  static constexpr size_t _sample_rate = 44100;
  static const size_t _vsamples = 1024;
  static const size_t _vsamples_chan = _vsamples / 2;
  float _vreal_left[_vsamples_chan] = {};
  float _vreal_right[_vsamples_chan] = {};
  float _vimag[_vsamples_chan] = {};
  float _wfactors[_vsamples_chan] = {};

  float _noise_filters[4][3] = {
      // {low_freq, high_freq, mag}
      {20.0, 58.0, 10.0},
      {58.0, 65.0, 9.0},
      {65.0, 150.0, 9.0},
      {150.0, 21000.0, 9.0}};

  float _mpeak = 0.0;
  float _mpeak_mag = 0.0;
  Derivative<float> _mag_history;

  bool _noise = true;
  bool _bass = false;
  float _bass_mag = 0.0;
  float _bass_mag_floor = 63.5;
  float _mag_floor = 38.5;

  ArduinoFFT _fft =
      ArduinoFFT(_vreal_left, _vimag, _vsamples_chan, _sample_rate, _wfactors);

  // UDP client
  const int _addr_family = AF_INET;
  const int _ip_protocol = IPPROTO_IP;
  const char *_host_ip = "192.168.2.67";
  const int _port_raw = 44100;
  const int _port_text = 44101;

  struct sockaddr_in _dest_raw = {};
  struct sockaddr_in _dest_text = {};
  int _socket_raw = -1;
  int _socket_text = -1;
  uint32_t _udp_errors = 0;

  esp_timer_handle_t _stats_timer = nullptr;
  const uint8_t _stats_interval_secs = 3;

  Task_t _task = {
      .handle = nullptr, .data = nullptr, .priority = 19, .stackSize = 4096};
};
} // namespace ruth

#endif
