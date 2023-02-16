
//  Ruth
//  Copyright (C) 2017  Tim Hughey
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

// override component logging level (must be #define before including esp_log.h)
// #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "ruth_mqtt/mqtt.hpp"
#include "filter/subscribe.hpp"
#include "message/handler.hpp"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <optional>

namespace ruth {

namespace shared {
DRAM_ATTR std::optional<MQTT> mqtt;
}

using namespace shared; // easy access to mqtt singleton
using namespace message;

static const char *TAG{"ruth_mqtt"};

MQTT::MQTT(ConnOpts &&opts, message::Handler *handler) noexcept
    : opts{std::move(opts)}, //
      conn{nullptr},         //
      mqtt_ready(false),     //
      sub_msg_id(0),         //
      broker_acks{0},        //
      handlers{}             //
{
  esp_log_level_set(TAG, ESP_LOG_INFO);
  esp_mqtt_client_config_t client_opts{};

  client_opts.broker.address.uri = opts.uri;
  client_opts.buffer.out_size = 5120;
  client_opts.buffer.size = 1024;
  client_opts.credentials.client_id = opts.client_id;
  client_opts.credentials.authentication.password = opts.passwd;
  client_opts.credentials.username = opts.user;
  client_opts.network.reconnect_timeout_ms = 30000;
  client_opts.session.disable_clean_session = true;
  client_opts.task.priority = 1;

  handlers[0] = handler;

  conn = esp_mqtt_client_init(&client_opts);
  esp_mqtt_client_register_event(conn, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
                                 &event_handler, conn);

  client_start_rc = esp_mqtt_client_start(conn);
}

void IRAM_ATTR MQTT::event_handler([[maybe_unused]] void *user_ctx, esp_event_base_t base,
                                   int32_t event_id,
                                   void *event_data) noexcept { // static

  auto *e = static_cast<esp_mqtt_event_handle_t>(event_data);

  switch (event_id) {
  case MQTT_EVENT_BEFORE_CONNECT:
    mqtt->self = xTaskGetCurrentTaskHandle(); // need for hold_for_connect()
    mqtt->sub_msg_id = 0;                     // reset subscribe msg id
    break;

  case MQTT_EVENT_CONNECTED: {
    esp_mqtt_connect_return_code_t status = e->error_handle->connect_return_code;
    ESP_LOGD(base, "CONNECT event_base=%s err_code[%d]", base, status);

    if (status == MQTT_CONNECTION_ACCEPTED) {
      xTaskNotify(mqtt->opts.notify_task, MQTT::CONNECTED, eSetBits);

      mqtt->subscribe();
    } else {
      ESP_LOGW(base, "mqtt connection error[%d]", status);
    }
  }

  break;

  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGW(base, "mqtt disconnected");
    break;

  case MQTT_EVENT_SUBSCRIBED:
    mqtt->subscribe_ack(base, e->msg_id);
    break;

  case MQTT_EVENT_DATA: {

    // ensure there is actually a payload to handle
    if (e->total_data_len > 0) {
      mqtt->incomingMsg(In::make(e->topic, e->topic_len, e->data, e->data_len));
    }
  } break;

  case MQTT_EVENT_PUBLISHED:
    mqtt->broker_acks++;
    break;

  default:
    ESP_LOGW(base, "unhandled event[0x%04lx]", event_id);
    break;
  }
}

bool MQTT::hold_for_connection(int32_t max_wait_ms) noexcept {

  // prevent waiting on ourself
  if (mqtt->self == xTaskGetCurrentTaskHandle()) return false;

  uint32_t notify;
  xTaskNotifyWait(0, ULONG_MAX, &notify, pdMS_TO_TICKS(max_wait_ms));

  if (notify & (MQTT::CONNECTED | MQTT::READY)) return true;

  ESP_LOGW(TAG, "connection timeout after %ldms", max_wait_ms);
  return false;
}

void IRAM_ATTR MQTT::incomingMsg(InWrapped msg) noexcept {
  bool wanted = false;

  for (auto registered : handlers) {
    if (registered == nullptr) break;

    if (registered->matchCategory(msg->category())) {
      registered->wantMessage(msg);

      if (msg->wanted()) {
        wanted = true;

        registered->accept(std::move(msg));
      }
    }
  }

  if (!wanted) {
    ESP_LOGW(TAG, "unwanted msg: %s", msg->category());
  }
}

void MQTT::registerHandler(message::Handler *handler) noexcept { // static
  for (auto &h : mqtt->handlers) {
    if (!h) {
      h = handler;
      break;
    }
  }
}

bool MQTT::send(message::Out &&msg) noexcept {
  size_t bytes;
  auto packed = msg.pack(bytes);
  auto msg_id = esp_mqtt_client_publish(mqtt->conn,   // queue to this connection
                                        msg.filter(), // destined to this topic
                                        packed.get(), // this data
                                        bytes,        // bytes to send
                                        0,            // qos
                                        false         // retain
  );

  // esp_mqtt_client_publish returns the msg_id on success, -1 if failed
  return (msg_id >= 0) ? true : false;
}

void MQTT::subscribe() noexcept {

  // create the subscribe topic (filter)
  const filter::Subscribe filter;

  sub_msg_id = esp_mqtt_client_subscribe(conn, filter.c_str(), 0 /* hard coded QoS 0 */);

  ESP_LOGD(TAG, "SUBSCRIBE TO filter[%s] msg_id[%d]", filter.c_str(), sub_msg_id);
}

void MQTT::subscribe_ack(const char *tag, int msg_id) noexcept {

  if (msg_id == sub_msg_id) {
    xTaskNotify(opts.notify_task, MQTT::READY, eSetBits);

    ESP_LOGD(tag, "SUBSCRIBE ACK msg_id[%u]", msg_id);

    if (esp_log_level_get(TAG) == ESP_LOG_DEBUG) {
      constexpr auto stack = CONFIG_MQTT_TASK_STACK_SIZE;
      const auto high_water = uxTaskGetStackHighWaterMark(nullptr);

      ESP_LOGD(tag, "MQTT READY stack[%u] highwater[%u]", stack, high_water);
    }

    // NOTE: do not announce startup here.  doing so creates a race condition
    // that results in occasionally using epoch as the startup time
  } else {
    ESP_LOGW(TAG, "SUBSCRIBE ACK for UNKNOWN msg_id[%d]", msg_id);
  }
}

} // namespace ruth
