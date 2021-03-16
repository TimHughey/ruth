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
#include "local/types.hpp"
#include "misc/maverage.hpp"
#include "misc/tracked.hpp"
#include "misc/valminmax.hpp"

namespace ruth {

typedef class I2s I2s_t;

class I2s {

public:
  struct Stats {

    struct {
      CountPerInterval raw_bps;
      float samples_per_sec;
      CountPerInterval fft_per_sec;
    } rate;

    ValMinMax<int_fast32_t> raw_val_left;
    ValMinMax<int_fast32_t> raw_val_right;
    ValMinMaxFloat_t dB;

    struct {
      float instant = 0;
      float avg7sec = 0;
    } complexity;

    struct {
      float freq_bin_width = 0;
      float dB_floor = 0;
      float bass_dB_floor = 0;
      float complexity_dB_floor = 0;
    } config;

    struct {
      elapsedMicrosTracked fft_us;
      elapsedMicrosTracked rx_us;
    } elapsed;

    struct {
      float freq = 0.0;
      float dB = 0.0;
    } mpeak;
  };

  typedef struct Stats Stats_t;

  typedef enum {
    NotifyStop = 0x01,
    NotifyShutdown = 0x02,
    NotifyStatsCalculate = 0x03
  } NotifyVal_t;

public:
  I2s();
  ~I2s();

  inline bool bass() const {
    bool bass = false;

    constexpr TickType_t wait_ticks = pdMS_TO_TICKS(3);
    if (xSemaphoreTake(_mutex, wait_ticks) == pdTRUE) {
      for (const PeakInfo &peak : _peaks) {
        // peaks are sorted in descending dB so it is safe to break
        // out of the loop once the first fails the check
        if (peak.dB < _bass_dB_floor) {
          break;
        } else if ((peak.freq > 30.0) && (peak.freq <= 170.0)) {
          bass = true;
          break;
        }
      }

      xSemaphoreGive(_mutex);
    }

    return bass;
  }

  float bassdBFloor() const { return _bass_dB_floor; }
  void bassdBFloor(const float floor) { _bass_dB_floor = floor; }

  void calculateComplexity();
  float complexity() const { return _complexity_mavg.lastValue(); }
  float complexityAvg() const { return _complexity_mavg.latest(); }
  size_t complexityAvgSize() const { return _complexity_mavg.size(); }
  float complexitydBFloor() const { return _complexity_dB_floor; }
  void complexitydBFloor(const float floor) { _complexity_dB_floor = floor; }

  float dBFloor() const { return _dB_floor; }
  void dBFloor(const float floor) { _dB_floor = floor; }
  inline bool dBResetTracking() {
    _stats.dB.reset();
    return true;
  }

  inline PeakInfo majorPeak() {
    constexpr TickType_t wait_ticks = pdMS_TO_TICKS(3);
    if (xSemaphoreTake(_mutex, wait_ticks) == pdTRUE) {
      PeakInfo &peak = ArduinoFFT::majorPeak(_peaks);
      xSemaphoreGive(_mutex);

      const dB_t dB = peak.dB;
      _stats.dB.track(dB);

      if (dB > _dB_floor) {
        return std::move(peak);
      }

    } else {
      printf("%s mutex timeout\n", __PRETTY_FUNCTION__);
    }

    return PeakInfo();
  }

  bool silence() const {
    bool rc = true;

    if (complexityAvg() < 5.8) {
      rc = false;
    }

    return rc;
  }

  // stats
  void stats(Stats_t &stats) {
    PeakInfo mpeak = majorPeak();

    _stats.mpeak.freq = mpeak.freq;
    _stats.mpeak.dB = mpeak.dB;
    _stats.config.dB_floor = _dB_floor;
    _stats.config.bass_dB_floor = _bass_dB_floor;
    _stats.config.complexity_dB_floor = _complexity_dB_floor;
    _stats.complexity.instant = complexity();
    _stats.complexity.avg7sec = complexityAvg();

    stats = _stats;
  }

  void start() { taskStart(); }
  void stop() { taskNotify(NotifyStop); }

private:
  typedef enum { INIT = 0x00, PROCESS_AUDIO, STOP, SHUTDOWN } I2sMode_t;

private:
  void fft(uint8_t *buffer, size_t len);

  bool handleNotifications();

  bool samplesRx();
  bool samplesUdpTx();

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
  static constexpr uint32_t _evq_max_depth = 4;
  bool _need_driver_install = true;

  uint8_t *_raw = nullptr;

  Stats_t _stats;

  static constexpr size_t _sample_rate = 44100;
  static const size_t _vsamples = 2048;
  static const size_t _vsamples_chan = _vsamples / 2;
  float _vreal_left[_vsamples_chan] = {};
  float _vreal_right[_vsamples_chan] = {};
  float _vimag[_vsamples_chan] = {};
  float _wfactors[_vsamples_chan] = {};

  float _bass_dB_floor = 77.5;
  float _complexity_dB_floor = 48.5;
  float _dB_floor = 64.2;

  MovingAverage<float, 7> _complexity_mavg;

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
  const uint8_t _stats_interval_secs = 1;

  Peaks_t _peaks;

  // protects shared access to mpeak and frequency
  portMUX_TYPE _spinlock = portMUX_INITIALIZER_UNLOCKED;

  StaticSemaphore_t _mutex_buff;
  SemaphoreHandle_t _mutex;

  Task_t _task = {
      .handle = nullptr, .data = nullptr, .priority = 19, .stackSize = 4096};
};
} // namespace ruth

#endif
