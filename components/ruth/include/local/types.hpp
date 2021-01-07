/*
    local/types.hpp - Ruth Local Types
    Copyright (C) 2017  Tim Hughey

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

#ifndef _ruth_local_types_hpp
#define _ruth_local_types_hpp

#include <memory>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "misc/textbuffer.hpp"

namespace ruth {

typedef TextBuffer<576> BinderRaw_t;
typedef TextBuffer<1024> BinderPrettyJson_t;
typedef TextBuffer<20> Hostname_t;
typedef TextBuffer<512> MsgPackPayload_t;
typedef TextBuffer<128> OtaUri_t;
typedef TextBuffer<20> PinSpotName_t;
typedef TextBuffer<40> RefID_t; // e.g. eaa7c7fa-361a-4d07-a7fc-fe9681636b36
typedef TextBuffer<CONFIG_FREERTOS_MAX_TASK_NAME_LEN> TaskName_t;
typedef TextBuffer<20> TimerName_t;
typedef TextBuffer<1024> WatcherPayload_t;

typedef const char *CSTR; // used for static string assignments
typedef uint32_t Rgbw_t;

// type passed to xTaskCreate as the function to run as a task
typedef void(TaskFunc_t)(void *);

typedef struct {
  TaskHandle_t handle;
  void *data;
  UBaseType_t priority;
  UBaseType_t stackSize;
} Task_t;

typedef enum { BINDER_CLI, BINDER_LIGHTDESK } BinderCategory_t;

namespace lightdesk {
typedef enum {
  fxNone = 0x00,
  fxPrimaryColorsCycle = 0x01,
  fxRedOnGreenBlueWhiteJumping = 0x02,
  fxGreenOnRedBlueWhiteJumping = 0x03,
  fxBlueOnRedGreenWhiteJumping = 0x04,
  fxWhiteOnRedGreenBlueJumping = 0x05,
  fxWhiteFadeInOut = 0x06,
  fxRgbwGradientFast = 0x07,
  fxRedGreenGradient = 0x08,
  fxRedBlueGradient = 0x09,
  fxBlueGreenGradient = 0x0a,
  fxFullSpectrumCycle = 0x0b,
  fxFullSpectrumJumping = 0x0c,
  fxColorCycleSound = 0x0d,
  fxColorStrobeSound = 0x0e,
  fxFastStrobeSound = 0x0f,
  fxUnknown = 0x10,
  fxColorBars = 0x11,
  fxWashedSound,
  fxSimpleStrobe,
  fxCrossFadeFast
} Fx_t;
} // namespace lightdesk

typedef struct {
  float fps = 0.0;
  uint64_t busy_wait = 0;
  uint64_t frame_update_us = 0;
  size_t object_size = 0;

  struct {
    uint64_t us = 0;
    uint64_t count = 0;
    uint64_t shorts = 0;

    struct {
      uint64_t curr = 0;
      uint64_t min = 9999;
      uint64_t max = 0;
    } update;
  } frame;

  struct {
    float curr = 0.0f;
    float min = 9999.0f;
    float max = 0.0f;
  } tx;

} DmxStats_t;

typedef struct {
  struct {
    uint64_t retries = 0;
    uint64_t failures = 0;
    uint64_t count = 0;
  } notify;
  size_t object_size = 0;
} PinSpotStats_t;

typedef struct {
  struct {
    uint64_t basic = 0;
    lightdesk::Fx_t active = lightdesk::fxNone;
    lightdesk::Fx_t next = lightdesk::fxNone;
    lightdesk::Fx_t prev = lightdesk::fxNone;
  } fx;

  struct {
    float base = 0.0f;
    float current = 0.0f;
    float min = 9999.9f;
    float max = 0.0f;
  } interval;

  size_t object_size = 0;
} LightDeskFxStats_t;

typedef struct {
  const char *mode = nullptr;
  size_t object_size = 0;
  DmxStats_t dmx;
  LightDeskFxStats_t fx;
  PinSpotStats_t pinspot[2];
} LightDeskStats_t;

typedef enum {
  NotifyZero = 0x0000,
  // timers that most likely firing at or near DMX frame rate
  NotifyTimer = 0x1001,
  NotifyFrame,
  NotifyFaderTimer,
  // notifications for commands
  NotifyQueue = 0x2001,
  NotifyColor,
  NotifyDark,
  NotifyDance,
  NotifyDanceExecute,
  NotifyFadeTo,
  // notifications for changing task operational mode
  NotifyStop = 0x3001,
  NotifyOff,
  NotifyPause,
  NotifyResume,
  NotifyReady,
  NotifyShutdown,
  NotifyDelete,
  NotifyEndOfValues
} NotifyVal_t;

namespace lightdesk {

typedef float Strobe_t;

typedef enum {
  // used to index into a color parts array
  RED_PART = 0,
  GREEN_PART,
  BLUE_PART,
  WHITE_PART,
  END_OF_PARTS
} ColorPart_t;

} // namespace lightdesk

} // namespace ruth
#endif // ruth_type_hpp
