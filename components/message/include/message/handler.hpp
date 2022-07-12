/*
  Message
  (C)opyright 2021  Tim Hughey

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

#pragma once

#include "message/in.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <memory>

namespace ruth {

namespace message {

class Handler {

public:
  Handler(const char *category, size_t const max_queue_depth);
  virtual ~Handler() = default;

  bool accept(InWrapped msg);

  bool matchCategory(const char *category) const;
  UBaseType_t notifyMessageValDefault() const { return notify_msg_val_default; }

  void notifyThisTask(UBaseType_t notify_val);
  TaskHandle_t notifyTask() const { return _notify_task; }

  virtual InWrapped waitForMessage() { return waitForMessage(portMAX_DELAY, nullptr); }
  virtual InWrapped waitForMessage(uint32_t wait_ms, bool *timeout = nullptr);
  virtual InWrapped waitForNotifyOrMessage(UBaseType_t *notified);
  virtual void wantMessage(InWrapped &msg) = 0;

public:
  static constexpr UBaseType_t notify_msg_val_default = 0x01 << 27;

protected:
  char _category[24] = {};
  QueueHandle_t _msg_q = nullptr;

  UBaseType_t _notify_msg_val = notify_msg_val_default;
  TaskHandle_t _notify_task = nullptr;
};

} // namespace message
} // namespace ruth
