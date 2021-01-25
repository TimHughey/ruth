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
#include "core/core.hpp"
#include "external/ArduinoJson.h"
#include "misc/restart.hpp"

namespace ruth {

using TR = reading::Text;

OTA::~OTA() {
  while (_task.handle != nullptr) {
    vTaskDelay(10);
  }
}

void OTA::cancel() {
  if (_ota_handle) { // clean up
    ESP_LOGI("OTA", "canceled");
    esp_https_ota_finish(_ota_handle);
    _ota_handle = nullptr;
  }

  MsgPayload_t_ptr msg(new MsgPayload_t("ota_cancel"));
  Core::queuePayload(move(msg));
}

void OTA::core() {
  while (_run_task) {
    _run_task = false;

    uint32_t v = 0;
    xTaskNotifyWait(0x00, ULONG_MAX, &v, portMAX_DELAY);
    NotifyVal_t notify_val = static_cast<NotifyVal_t>(v);

    switch (notify_val) {
    case NotifyOtaStart:
      perform();
      break;

    case NotifyOtaCancel:
      cancel();
      break;

    case NotifyOtaFinish:
      Restart("OTA finished in %0.2fs, restarting", (float)_elapsed);
      break;

    default:
      break;
    }
  }
}

void OTA::coreTask(void *task_data) {
  OTA_t *ota = (OTA_t *)task_data;
  ota->core();

  TaskHandle_t task = ota->taskHandle();
  ota->taskHandle() = nullptr;

  ESP_LOGI("OTA", "task ending...");
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
    taskNotify(NotifyOtaStart);
  } else {
    taskNotify(NotifyOtaCancel);
  }

  return rc;
}

bool OTA::isNewImage(const esp_app_desc_t *asis, const esp_app_desc_t *tobe) {
  const size_t bytes = sizeof(asis->app_elf_sha256);
  const uint8_t *sha1 = asis->app_elf_sha256;
  const uint8_t *sha2 = tobe->app_elf_sha256;

  bool rc = false;

  if (memcmp(sha1, sha2, bytes) != 0) {
    rc = true;
  }

  TR::rlog("OTA image version=\"%s\" %s", tobe->version,
           (rc) ? "will be installed" : "is already installed");

  return rc;
}

bool OTA::perform() {
  auto rc = true;

  _run_task = true;

  const esp_partition_t *ota_part = esp_ota_get_next_update_partition(nullptr);
  esp_https_ota_config_t ota_config;
  esp_http_client_config_t http_conf = {};
  http_conf.url = _uri.c_str();
  http_conf.cert_pem = Net::ca_start();
  http_conf.event_handler = OTA::httpEventHandler;
  http_conf.timeout_ms = 1000;

  ota_config.http_config = &http_conf;

  // track the time it takes to perform ota
  _elapsed.reset();

  auto esp_rc = esp_https_ota_begin(&ota_config, &_ota_handle);

  const esp_app_desc_t *app_curr = esp_ota_get_app_description();
  esp_app_desc_t app_new;
  auto img_rc = esp_https_ota_get_img_desc(_ota_handle, &app_new);

  if (isNewImage(app_curr, &app_new) && (img_rc == ESP_OK)) {
    // this is a new image
    TR::rlog("OTA begin partition=\"%s\" addr=0x%x", ota_part->label,
             ota_part->address);

    auto ota_finish_rc = ESP_FAIL;

    if ((esp_rc == ESP_OK) && _ota_handle) {
      do {
        esp_rc = esp_https_ota_perform(_ota_handle);
      } while (esp_rc == ESP_ERR_HTTPS_OTA_IN_PROGRESS);

      ota_finish_rc = esp_https_ota_finish(_ota_handle);
    }

    // give priority to errors from ota_begin() vs. ota_finish()
    const auto ota_rc = (esp_rc != ESP_OK) ? esp_rc : ota_finish_rc;
    if (ota_rc == ESP_OK) {
      taskNotify(NotifyOtaFinish);
    } else {
      TR::rlog("[%s] OTA failure", esp_err_to_name(ota_rc));
      rc = false;
    }
  } else if (img_rc == ESP_OK) {
    // is is not a new image, clean up the ota_begin and return success
    taskNotify(NotifyOtaCancel);
    rc = true;
  } else {
    // failure with initial esp_begin
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

      TR::rlog("found pending partition, starting validate timer");

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
        TR::rlog("partition=\"%s\" marked as valid", run_part->label);
      } else {
        TR::rlog("[%s] failed to mark partition=\"%s\" as valid",
                 esp_err_to_name(mark_valid_rc), run_part->label);
      }
    }
  } else {
    TR::rlog("%s failed to get state of run_part=%p", __PRETTY_FUNCTION__,
             run_part);
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
