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

#ifndef _ruth_dmx512_hpp
#define _ruth_dmx512_hpp

#include <esp_event.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_timer.h>
#include <soc/uart_reg.h>

#include "devs/dmx/headunit.hpp"
#include "local/types.hpp"
#include "misc/elapsed.hpp"

namespace ruth {

typedef class Dmx Dmx_t;

class Dmx {

public:
  static Dmx_t *instance();
  ~Dmx();

  static uint64_t frameInterval() { return instance()->_frame_us; }
  static float frameIntervalAsSeconds() {
    const double frame_us = static_cast<double>(instance()->_frame_us);
    const float frame_secs = (frame_us / (1000.0f * 1000.0f));

    return frame_secs;
  }

  float framesPerSecond() const { return _stats.fps; }

  static bool isRunning();

  void registerHeadUnit(HeadUnit_t *unit);

  // stats
  void stats(DmxStats_t &stats) {
    stats = _stats;

    stats.frame.us = _frame_us;
    stats.object_size = sizeof(Dmx_t);
  }

  // task control
  static void start() { instance()->taskStart(); }
  static void stop();

  static TaskHandle_t taskHandle() { return instance()->_task.handle; }

private:
  typedef enum { INIT = 0x00, STREAM_FRAMES, PAUSE, STOP, SHUTDOWN } DmxMode_t;

private:
  Dmx(); // singleton, constructor is private

  inline void busyWait(uint32_t usec, const bool reset = true) {
    if (reset) {
      _mab_elapsed.reset();
    }

    while (_mab_elapsed <= usec) {
      _stats.busy_wait++;
      // yield to higher priority tasks to minimize impact of busy wait
      taskYIELD();
    };
  }

  static void fpsCalculate(void *data);
  void frameApplyUpdates();
  static void frameTimerCallback(void *data);
  esp_err_t frameTimerStart() const;

  void setMode(DmxMode_t mode);
  int txBytes();
  void txFrame();
  esp_err_t uartInit();

  // task implementation

  inline TaskHandle_t task() const { return _task.handle; }
  static void taskCore(void *task_instance);
  void taskLoop();
  BaseType_t taskNotify(NotifyVal_t nval);
  void taskStart();

  void waitForStop();

private:
  uint64_t _pin_sel = GPIO_SEL_17;
  gpio_config_t _pin_cfg = {};
  gpio_num_t _tx_pin = GPIO_NUM_17;
  int _uart_num = UART_NUM_1;
  esp_err_t _init_rc = ESP_FAIL;

  DmxMode_t _mode = INIT;
  elapsedMicros _mab_elapsed;
  static const size_t _frame_len = 127;
  uint8_t _frame[_frame_len] = {}; // the DMX frame starts as all zeros
  esp_timer_handle_t _frame_timer = nullptr;

  // except for _frame_break all frame timings are in µs
  const uint32_t _frame_break = 11; // num bits at 250,000 baud (4µs)
  const uint64_t _frame_mab = 12;
  const uint64_t _frame_byte = 44;
  const uint64_t _frame_sc = _frame_byte;
  const uint64_t _frame_mtbf = 44;
  const uint64_t _frame_data = _frame_byte * 512;
  // frame interval does not include the BREAK as it is handled by the UART
  uint64_t _frame_us = _frame_mab + _frame_sc + _frame_data + _frame_mtbf;

  const size_t _tx_buff_len = (_frame_len < 128) ? 0 : _frame_len + 1;

  elapsedMillis runtime_;

  esp_timer_handle_t _fps_timer = nullptr;
  uint64_t _frame_count_mark = 0;
  int _fpc_period = 3; // period represents seconds to count frames
  int _fpcp = 0;       // frames per calculate period

  HeadUnit_t *_headunit[10] = {};
  uint32_t _headunits = 0;

  DmxStats_t _stats;

  Task_t _task = {
      .handle = nullptr, .data = nullptr, .priority = 19, .stackSize = 4096};
};

} // namespace ruth

#endif
