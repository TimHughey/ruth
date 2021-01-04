/*
    devs/pinspot]/base.cpp - Ruth Pinspot Device
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

#include <esp_log.h>

#include "devs/dmx/pinspot/base.hpp"

namespace ruth {
namespace lightdesk {

PinSpot::PinSpot(uint16_t address, uint64_t frame_interval)
    : HeadUnit(address, 6) {

  _frame_us = frame_interval;
  start();
}

PinSpot::~PinSpot() {
  // not implemented, deleting a pinspot is very likely to trigger an abort
}

void PinSpot::autoRun(Fx_t fx) {
  setMode(HOLD);
  _fx = fx;
  setMode(AUTORUN);
}

uint8_t PinSpot::autorunMap(Fx_t fx) const {
  static const uint8_t model_codes[] = {0,   31,  63,  79,  95,  111, 127, 143,
                                        159, 175, 191, 207, 223, 239, 249, 254};

  const uint8_t model_num = static_cast<uint8_t>(fx);

  uint8_t selected_model = 0;

  if (model_num < sizeof(model_codes)) {
    selected_model = model_codes[model_num];
  }

  return selected_model;
}

void PinSpot::black() {
  setMode(HOLD);

  _color = Color::black();
  _strobe = 0.0f;

  setMode(COLOR);
}

void PinSpot::color(const Color_t &color, float strobe) {
  setMode(HOLD);

  _color = color;

  if ((strobe >= 0.0) && (strobe <= 1.0)) {
    _strobe = (uint8_t)(_strobe_max * strobe);
  }

  setMode(COLOR);
}

void PinSpot::dark() { setMode(DARK); }

void IRAM_ATTR PinSpot::core() {
  while (_running) {
    uint32_t notify_val;

    xTaskNotifyWait(0x00, ULONG_MAX, &notify_val, portMAX_DELAY);

    NotifyVal_t val = static_cast<NotifyVal_t>(notify_val);

    switch (val) {
    case NotifyFaderTimer:
      handleFastNotification(val);
      break;

      // always handle task control notifications
    case NotifyPause:
    case NotifyReady:
    case NotifyResume:
    case NotifyShutdown:
      handleTaskNotification(val);
      break;

    default:
      printf("pinspot unhandled notification: 0x%04x\n", val);
      break;
    }
  }
}

void PinSpot::coreTask(void *task_instance) {
  PinSpot_t *pinspot = (PinSpot_t *)task_instance;

  pinspot->core();

  // if core() ever returns then the task will become idle waiting forever
  vTaskDelay(portMAX_DELAY);
}

void PinSpot::faderMove() {
  auto continue_traveling = _fader.travel();
  _color = _fader.location();
  _strobe = 0;

  updateFrame();

  if (continue_traveling == false) {
    setMode(HOLD);
  } else {
    faderTimerStart();
  }
}

void PinSpot::fadeTo(const Color_t &dest, float secs, float accel) {
  const FaderOpts todo{
      .origin = _color, .dest = dest, .travel_secs = secs, .accel = accel};
  fadeTo(todo);
}

void PinSpot::fadeTo(const FaderOpts_t &fo) {
  setMode(HOLD);

  if (fo.use_origin) {
    // when use_origin is specified then do not pass the current color
    _fader.prepare(fo);
  } else {
    // otherwise the fader origin is the current color
    _fader.prepare(_color, fo);
  }

  setMode(FADER);
}

void IRAM_ATTR PinSpot::faderCallback(void *data) {
  PinSpot_t *pinspot = (PinSpot_t *)data;

  // fader timer is one shot. when this function is called we
  // are assured the timer is not running
  pinspot->faderTimerRunning() = false;
  pinspot->taskNotify(NotifyFaderTimer);
}

void IRAM_ATTR PinSpot::faderTimerStart() {
  if (_fader_timer && _normal_ops && (_mode == FADER)) {

    faderTimerStop();

    _fader_timer_running = true;
    esp_timer_start_once(_fader_timer, _frame_us);
  }
}

void IRAM_ATTR PinSpot::faderTimerStop() {
  if (_fader_timer && _fader_timer_running) {
    esp_timer_stop(_fader_timer);
  }
}

void PinSpot::handleFastNotification(NotifyVal_t val) {

  // quietly ignore "fast" notifications when not operating normally
  if (_normal_ops == true) {
    switch (val) {
    case NotifyFaderTimer:
      faderMove();
      break;

    default:
      break;
    }
  }
}

void PinSpot::handleTaskNotification(NotifyVal_t val) {
  switch (val) {
  case NotifyPause:
    _normal_ops = false; // normal_ops = false allows only task control
    break;

  case NotifyResume:
    _normal_ops = true;
    break;

  default:
    break;
  }
}

void PinSpot::morse(const char *text, uint32_t rgbw, uint32_t ms) {
  // setMode(MORSE);
}

void PinSpot::morseRender() {}

void PinSpot::setMode(Mode_t mode) {
  _mode = mode;

  switch (mode) {
  case DARK:
  case HOLD:
  case COLOR:
  case AUTORUN:
  case MORSE:
    break;

  case FADER:
    faderTimerStart();
    break;

  case SHUTDOWN:
    faderTimerStop();
    break;
  }

  updateFrame();
}

void PinSpot::resume() { taskNotify(NotifyResume); }
void PinSpot::pause() { taskNotify(NotifyPause); }

void PinSpot::start() {
  PinSpotName_t name;
  name.printf("pinspot.%02d", _address);
  if (_task.handle == nullptr) {
    // this (object) is passed as the data to the task creation and is
    // used by the static coreTask method to call cpre()
    ::xTaskCreate(&coreTask, name.c_str(), _task.stackSize, this,
                  _task.priority, &(_task.handle));
  }

  TimerName_t timer_name;
  name.printf("spot.%02d-fader", _address);

  if (_fader_timer == nullptr) {
    const esp_timer_create_args_t fader_timer_args = {
        .callback = faderCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = timer_name.c_str()};

    _init_rc = esp_timer_create(&fader_timer_args, &_fader_timer);
  }
}

bool PinSpot::taskNotify(NotifyVal_t val) {
  auto rc = true; // initial assumption is the notification will succeed
  TickType_t wait_ticks = 0;
  UBaseType_t notify_rc;

  for (wait_ticks = 0; wait_ticks <= _notify_max_ticks; wait_ticks++) {
    const uint32_t nval = static_cast<uint32_t>(val);

    // eSetValueWithoutOverwrite ensures xTaskNotify will not overwrite
    // a pending notification and returns pdFAIL if it would
    notify_rc = xTaskNotify(task(), nval, eSetValueWithoutOverwrite);

    // a notification is pending
    if (notify_rc == pdFAIL) {
      // wait for a single tick and retry
      vTaskDelay(1);

      _stats.notify.retries++;
      continue;
    } else if (notify_rc == pdPASS) {
      // notification posted, track stats and break out

      break;
    }
  }

  // if we've reached this point and notify_rc is pdFAIL then the
  // notification was not sent so track it in the stats
  if (notify_rc == pdFAIL) {
    rc = false;
    _stats.notify.failures++;
  }

  _stats.notify.count++;

  return rc;
}

void PinSpot::updateFrame() {
  uint8_t *data = frameData(); // HeadUnit member function
  const uint8_t autorun_byte = autorunMap(_fx);
  auto need_frame_update = true;

  _color.copyToByteArray(&(data[1])); // color data bytes 1-5

  switch (_mode) {
  case HOLD:
    need_frame_update = false;
    break;

  case DARK:
    // send frame initialized with head control == 0x00
    break;

  case COLOR:
    if (_strobe > 0) {
      data[0] = _strobe + 0x87;
    } else {
      data[0] = 0xF0;
    }
    break;

  case FADER:
    data[0] = 0xF0;
    break;

  case MORSE:
    data[0] = 0xF0; // only solid colors, no strobe
    break;

  case AUTORUN:
    // only update the frame if the autorun fx has changed
    if (data[5] == autorun_byte) {
      need_frame_update = false;
    } else {
      data[5] = autorunMap(_fx);
    }
    break;

  case SHUTDOWN:
    need_frame_update = false;
    break;
  }

  if (need_frame_update) {
    // the changes to the frame for this PinSpot were staged above.
    // calling frameChanged() will alert Dmx to incorporate the staged
    // changes into the next frame TXed
    frameChanged() = true;
  }
}

} // namespace lightdesk
} // namespace ruth