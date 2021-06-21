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

#ifndef message_handler_hpp
#define message_handler_hpp

#include <memory>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "in.hpp"

namespace message {

class Handler {
public:
  Handler(size_t max_queue_depth = 5);
  virtual ~Handler() = default;

  bool accept(InWrapped msg);

  InWrapped waitForMessage() { return waitForMessage(UINT32_MAX, nullptr); }
  virtual InWrapped waitForMessage(uint32_t wait_ms, bool *timeout = nullptr);
  virtual void wantMessage(InWrapped &msg) = 0;

protected:
  QueueHandle_t _msg_q = nullptr;
};

} // namespace message

#endif
