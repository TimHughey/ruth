/*
    cmd_ota.cpp - Ruth OTA Command Processing
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

#include "core/ota.hpp"
#include "core/binder.hpp"
#include "external/ArduinoJson.h"
#include "misc/restart.hpp"

namespace ruth {

using TR = reading::Text;

void OTA::core() {
  // wait for notification to start
  uint32_t v = 0;
  xTaskNotifyWait(0x00, ULONG_MAX, &v, pdMS_TO_TICKS(portMAX_DELAY));
  NotifyVal_t notify_val = static_cast<NotifyVal_t>(v);

  switch (notify_val) {
  case NotifyOtaStart:
    perform();
    break;

  case NotifyOtaCancel:
  default:
    TR::rlog("OTA cancelled");
    break;
  }
}

OTA::~OTA() {
  while (_task.handle != nullptr) {
    vTaskDelay(10);
  }
}

void OTA::coreTask(void *task_data) {
  OTA_t *ota = (OTA_t *)task_data;
  ota->core();

  TaskHandle_t task = ota->taskHandle();
  ota->taskHandle() = nullptr;
  vTaskDelete(task); // remove task from scheduler
}

bool OTA::handleCommand(MsgPayload_t &msg) {
  auto rc = true; // initial assumption is success

  const size_t _capacity = JSON_OBJECT_SIZE(3) + 336;
  StaticJsonDocument<_capacity> doc;
  // deserialize the msgpack data, use data() (returns char *)
  // to allow deserializeMsgPack() to minimize copying data
  //
  DeserializationError err = deserializeMsgPack(doc, msg.data());

  if (err) {
    TR::rlog("failed to parse OTA message: %s", err.c_str());
    rc = false;
  } else {
    _uri = doc["uri"] | "";

    if (strlen(_uri) < 3) {
      rc = false;
    }
  }

  if (rc) {
    xTaskNotify(taskHandle(), NotifyOtaStart, eSetValueWithOverwrite);
  } else {
    xTaskNotify(taskHandle(), NotifyOtaCancel, eSetValueWithOverwrite);
  }

  return rc;
}

bool OTA::perform() {
  auto rc = true;

  const esp_partition_t *ota_part = esp_ota_get_next_update_partition(nullptr);
  esp_http_client_config_t config = {};
  config.url = _uri.c_str();
  config.cert_pem = Net::ca_start();
  config.event_handler = OTA::httpEventHandler;
  config.timeout_ms = 1000;

  TR::rlog("OTA begin partition=\"%s\" addr=0x%x", ota_part->label,
           ota_part->address);

  // track the time it takes to perform ota
  elapsedMicros ota_elapsed;
  esp_err_t esp_rc = esp_https_ota(&config);

  if (esp_rc == ESP_OK) {
    const float secs = (float)ota_elapsed;
    TextBuffer<35> msg;

    msg.printf("OTA complete, elapsed=%0.2fs, restarting", secs);
    Restart(msg.c_str());
  } else {
    TR::rlog("[%s] OTA failure", esp_err_to_name(esp_rc));
    rc = false;
  }

  return rc;
}

void OTA::start() {
  // ignore requets if the task is already running
  if (_task.handle != nullptr) {
    TR::rlog("OTA already in-progress");
    return;
  }

  // this (object) is passed as the data to the task creation and is
  // used by the static runEngine method to call the run method
  xTaskCreate(&coreTask, "OTATask", _task.stackSize, this, _task.priority,
              &_task.handle);
}

void OTA::partitionHandlePendingIfNeeded() {
  const esp_partition_t *run_part = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;

  if (esp_ota_get_state_partition(run_part, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      const auto valid_ms = Binder::otaValidMs();

      auto timer = xTimerCreate("ota_validate", pdMS_TO_TICKS(valid_ms),
                                pdFALSE, nullptr, &partitionMarkValid);
      xTimerStart(timer, pdMS_TO_TICKS(0));
    }
  }
}

void OTA::partitionMarkValid(TimerHandle_t handle) {
  using TR = reading::Text;

  const esp_partition_t *run_part = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;

  if (esp_ota_get_state_partition(run_part, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_err_t mark_valid_rc = esp_ota_mark_app_valid_cancel_rollback();

      if (mark_valid_rc == ESP_OK) {
        TR::rlog("[%s] partition=\"%s\" marked as valid",
                 esp_err_to_name(mark_valid_rc), run_part->label);
      } else {
        TR::rlog("[%s] failed to mark partition=\"%s\" as valid",
                 esp_err_to_name(mark_valid_rc), run_part->label);
      }
    }
  }

  xTimerDelete(handle, 0);
}

//
// STATIC!
//
esp_err_t OTA::httpEventHandler(esp_http_client_event_t *evt) {

  switch (evt->event_id) {
  case HTTP_EVENT_ON_HEADER:
  case HTTP_EVENT_ERROR:
  case HTTP_EVENT_ON_CONNECTED:
  case HTTP_EVENT_HEADER_SENT:
  case HTTP_EVENT_ON_DATA:
  case HTTP_EVENT_ON_FINISH:
  case HTTP_EVENT_DISCONNECTED:
    break;
  }
  return ESP_OK;
}

} // namespace ruth
