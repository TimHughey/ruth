
//  Ruth
//  Copyright (C) 2021  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#include "message/handler.hpp"

#include <esp_log.h>

namespace ruth {
namespace message {

Handler::Handler(const char *category, const size_t max_queue_depth) {
  memccpy(_category, category, 0x00, sizeof(_category) - 1);
  _msg_q = xQueueCreate(max_queue_depth, sizeof(In *));
}

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

    if (_notify_task) {
      xTaskNotify(_notify_task, _notify_msg_val, eSetBits);
    }

    return true;
  }

  // second queue attempt failed
  return false;
}

bool Handler::matchCategory(const char *to_match) const {
  return strncmp(_category, to_match, sizeof(_category)) == 0 ? true : false;
}

InWrapped Handler::waitForMessage(uint32_t wait_ms, bool *timeout) {
  In *received_msg;

  auto q_rc = xQueueReceive(_msg_q, &received_msg, pdMS_TO_TICKS(wait_ms));

  if (q_rc == pdTRUE) {
    // got a message from the queue, wrap it in a unique_ptr and return
    if (timeout) *timeout = false;

    return InWrapped(received_msg);
  }

  // note a timeout occurred and return the empty unique_ptr
  if (timeout) *timeout = true;
  return InWrapped(nullptr);
}

void Handler::notifyThisTask(UBaseType_t notify_val) {
  _notify_task = xTaskGetCurrentTaskHandle();
  _notify_msg_val = notify_val;
}

InWrapped Handler::waitForNotifyOrMessage(UBaseType_t *notified) {
  In *received_msg = nullptr;
  // always do a no wait check for messages in the queue

  auto q_rc = xQueueReceive(_msg_q, &received_msg, 0);

  // if we got a message from the queue, then return it without waiting for a task notify
  if (q_rc == pdTRUE) {
    ESP_LOGI("message:handler", "msg was waiting in the queue");
    return InWrapped(received_msg);
  }

  // wait for a task notification.  on any notification do a no wait queue pop and return
  // whatever was popped (or not popped)
  xTaskNotifyWait(0x00, portMAX_DELAY, notified, portMAX_DELAY);
  ESP_LOGD("message:handler", "notified 0x%0x", *notified);

  // pop it from the queue and return it
  // always do a no wait check for messages in the queue
  xQueueReceive(_msg_q, &received_msg, 0);
  return InWrapped(received_msg);
}

} // namespace message
} // namespace ruth
