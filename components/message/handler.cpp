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

#include "message/handler.hpp"

namespace message {
Handler::Handler(size_t max_queue_depth) { _msg_q = xQueueCreate(max_queue_depth, sizeof(In *)); }

bool Handler::accept(std::unique_ptr<In> msg) {
  auto q_rc = pdFALSE;

  In *msg_for_q = msg.get();

  q_rc = xQueueSendToBack(_msg_q, (void *)&msg_for_q, 0);

  if (q_rc == errQUEUE_FULL) {
    // the queue is full, remove the oldest to make space
    In *oldest;
    q_rc = xQueueReceive(_msg_q, &oldest, 0);
    if (q_rc == pdTRUE) {
      delete oldest;
    }

    // now attempt to queue again
    q_rc = xQueueSendToBack(_msg_q, (void *)&msg_for_q, 0);
  }

  // if either of the attempts to put the message on the queue succeeded then release the
  // message from the unique_ptr
  if (q_rc == pdTRUE) {
    msg.release();
    return true;
  }

  // second queue attempt failed
  return false;
}

std::unique_ptr<In> Handler::waitForMessage(uint32_t wait_ms, bool *timeout) {

  std::unique_ptr<In> return_msg(nullptr);
  In *received_msg;

  auto q_rc = xQueueReceive(_msg_q, &received_msg, pdMS_TO_TICKS(wait_ms));

  if (q_rc == pdTRUE) {
    // got a message from the queue, wrap it in a unique_ptr and return
    if (timeout) *timeout = false;

    return_msg = std::unique_ptr<In>(received_msg);

    return std::move(return_msg);
  }

  // note a timeout occurred and return the empty unique_ptr
  if (timeout) *timeout = true;
  return std::move(return_msg);
}

} // namespace message
