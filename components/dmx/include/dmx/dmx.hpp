/*
    protocols/dmx.hpp - Ruth Dmx Protocol Engine
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

#ifndef dmx512_hpp
#define dmx512_hpp

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>

#include <array>
#include <string>
#include <unordered_set>

#include "dmx/packet.hpp"
#include "headunit/headunit.hpp"

namespace dmx {

class Dmx {
  static constexpr size_t _dmx_frame_len = 384;

  typedef std::array<uint8_t, _dmx_frame_len> DmxFrame;
  typedef enum { INIT = 0x00, STREAM_FRAMES, SHUTDOWN } DmxMode_t;

public:
  struct Stats {
    float fps = 0.0;

    struct {
      uint64_t count = 0;
      uint64_t shorts = 0;
    } frame;
  };

public:
  Dmx(const uint32_t dmx_port);
  ~Dmx();

  Dmx(const Dmx &) = delete;
  Dmx &operator=(const Dmx &) = delete;
  void addHeadUnit(spHeadUnit hu) { _headunits.insert(hu); }

  inline float fpsExpected() const {
    constexpr float seconds_us = 1000.0f * 1000.0f;
    const float frame_us = static_cast<float>(_frame_us);
    return (seconds_us / frame_us);
  }

  inline uint64_t frameInterval() const { return _frame_us; }
  inline float frameIntervalAsSeconds() {
    const float frame_us = static_cast<float>(_frame_us);
    const float frame_secs = (frame_us / (1000.0f * 1000.0f));

    return frame_secs;
  }

  float framesPerSecond() const { return _stats.fps; }

  inline HeadUnitTracker &headunits() { return _headunits; }

  inline float idle() const { return _stats.fps == 0.0f; }

  inline static Dmx *instance() { return _instance; }

  // task control
  void start() { taskStart(); }
  void stop();

private:
  static void fpsCalculate(void *data);

  void txFrame(const dmx::Packet &packet);
  esp_err_t uartInit();

  // task implementation
  // inline TaskHandle_t task() const { return _task.handle; }
  static void taskCore(void *task_instance);
  void taskInit();
  void taskLoop();
  void taskStart();

private:
  uint32_t _udp_port;
  int _socket = -1;
  const int _uart_num;
  esp_err_t _init_rc = ESP_FAIL;

  DmxMode_t _mode = INIT;
  DmxFrame _frame; // the DMX frame starts as all zeros

  // except for _frame_break all frame timings are in µs
  const uint_fast32_t _frame_break = 22; // num bits at 250,000 baud (8µs)
  const uint_fast32_t _frame_mab = 12;
  const uint_fast32_t _frame_byte = 44;
  const uint_fast32_t _frame_sc = _frame_byte;
  const uint_fast32_t _frame_mtbf = 44;
  const uint_fast32_t _frame_data = _frame_byte * 512;
  // frame interval does not include the BREAK as it is handled by the UART
  uint64_t _frame_us = _frame_mab + _frame_sc + _frame_data + _frame_mtbf;

  // UART tx buffer size calculation
  const size_t _tx_buff_len = (_dmx_frame_len < 128) ? 0 : _dmx_frame_len + 1;
  esp_timer_handle_t _fps_timer = nullptr;
  uint64_t _frame_count_mark = 0;
  int _fpc_period = 2; // period represents seconds to count frames
  int _fpcp = 0;       // frames per calculate period

  HeadUnitTracker _headunits;

  Stats _stats;

  static Dmx *_instance;
};

} // namespace dmx

#endif
