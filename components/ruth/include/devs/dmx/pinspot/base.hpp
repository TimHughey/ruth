/*
    devs/pinspot/base.hpp - Ruth Pin Spot Device
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

#ifndef _ruth_pinspot_device_base_hpp
#define _ruth_pinspot_device_base_hpp

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include "devs/dmx/headunit.hpp"
#include "devs/dmx/pinspot/color.hpp"
#include "devs/dmx/pinspot/fader.hpp"
#include "lightdesk/fx_defs.hpp"
#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "protocols/dmx.hpp"

namespace ruth {
namespace lightdesk {

typedef class PinSpot PinSpot_t;

class PinSpot : public HeadUnit {
public:
  PinSpot(uint16_t address = 1, uint64_t frame_interval = 22754);
  ~PinSpot();

  void autoRun(Fx_t fx);
  void black();
  void color(int r, int g, int b, int w) { color(Color(r, g, b, w)); }
  void color(const Color_t &color, float strobe = 0.0);
  void dark();
  void fadeTo(const Color_t &color, float secs = 1.0, float accel = 0.0);
  void fadeTo(const FaderOpts_t &opts);

  void morse(const char *text, uint32_t rgbw = 0xff, uint32_t ms = 10);

  // task control
  void stop() { taskNotify(NotifyStop); }

  void stats(PinSpotStats_t &stats) const {
    stats = _stats;
    stats.object_size = sizeof(PinSpot_t);
  }

  typedef enum {
    AUTORUN = 0x3000,
    DARK,
    COLOR,
    FADER,
    HOLD,
    MORSE,
    READY,
    STOP = 0x3100,
    SHUTDOWN
  } Mode_t;

private:
  // functions
  uint8_t autorunMap(Fx_t fx) const;

  inline bool faderFinished() const { return _fader.finished(); };
  void faderMove();
  void faderStart(const FaderOpts &opts);

  static void faderCallback(void *data);
  bool &faderTimeActive() { return _fader_timer_active; }
  void faderTimerStart();
  void faderTimerStop();

  void makeFrame(uint8_t *data);
  void morseRender();

  void setMode(Mode_t mode);
  void setRgbw(uint32_t color);

  inline TaskHandle_t task() const { return _task.handle; }
  inline const char *taskName() const { return pcTaskGetTaskName(nullptr); }

  bool taskNotify(NotifyVal_t val);

  void updateFrame();

  // task
  void core();
  static void coreTask(void *task_instance);
  void start();

private:
  esp_err_t _init_rc = ESP_FAIL;
  bool _normal_ops = true; // false = handling task notification

  // task
  Task_t _task = {
      .handle = nullptr, .data = nullptr, .priority = 18, .stackSize = 4096};

  // update frame spinlock
  portMUX_TYPE _spinlock = portMUX_INITIALIZER_UNLOCKED;

  // high-res timers
  esp_timer_handle_t _fader_timer = nullptr;
  bool _fader_timer_active = false;

  // max ticks before failing to notify task
  const TickType_t _notify_max_ticks = pdMS_TO_TICKS(100);

  Mode_t _mode = DARK;

  Color_t _color;
  uint8_t _strobe = 0;
  uint8_t _strobe_max = 104;
  Fx_t _fx = fxNone;

  uint64_t _frame_us = 0;
  uint64_t _morse_element_us = 10 * 1000; // 10ms

  PinSpotStats_t _stats;

  Fader_t _fader;
};
} // namespace lightdesk
} // namespace ruth

#endif
