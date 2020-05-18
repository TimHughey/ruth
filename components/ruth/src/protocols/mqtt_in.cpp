/*
    mqtt_out.cpp - Ruth MQTT Outbound Message Task
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

#include <cstdlib>
#include <cstring>
#include <vector>

#include <esp_log.h>
#include <forward_list>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>

#include "cmds/factory.hpp"
#include "misc/local_types.hpp"
#include "misc/profile.hpp"
#include "net/network.hpp"
#include "protocols/mqtt_in.hpp"
#include "readings/readings.hpp"

namespace ruth {

static char TAG[] = "MQTTin";

static MQTTin_t *__singleton = nullptr;

// NOTE:  should be set to appropriate size based on ArduinoJson
static const size_t _doc_capacity = 768;

MQTTin::MQTTin(QueueHandle_t q_in, const char *cmd_feed)
    : _q_in(q_in), _cmd_feed(cmd_feed) {
  esp_log_level_set(TAG, ESP_LOG_INFO);

  ESP_LOGD(TAG, "task created, queue(%p)", (void *)_q_in);
  __singleton = this;
}

MQTTin_t *MQTTin::_instance_() { return __singleton; }

void MQTTin::core(void *data) {
  MsgPayload *msg;
  CmdFactory_t factory;
  DynamicJsonDocument doc(_doc_capacity); // allocate the document here

  // note:  no reason to wait for wifi, normal ops or other event group
  //        bits since MQTTin waits for queue data from other tasks via
  //        MQTT::publish().
  //
  //        said differently, this task will not execute until another task
  //        sends it something through the queue.
  ESP_LOGD(TAG, "started, entering run loop");

  for (;;) {
    bzero(&msg, sizeof(MsgPayload *)); // just because we like clean memory

    BaseType_t q_rc = xQueueReceive(_q_in, &msg, portMAX_DELAY);

    if (q_rc == pdTRUE) {
      // only deprecated messages without a subtopic are queued so
      // processe the payload via the command factory
      Cmd_t *cmd = factory.fromRaw(doc, msg->payload());

      if (cmd && cmd->recent() && cmd->forThisHost()) {
        Cmd_t_ptr cmd_ptr(cmd);
        cmd->process();
      }

      // ok, we're done with the message
      delete msg;
    } else {
      ESP_LOGW(TAG, "queue received failed");
      continue;
    }
  } // infinite loop to process inbound MQTT messages
}
} // namespace ruth
