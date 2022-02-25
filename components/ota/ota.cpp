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

#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_spi_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include "ota/ota.hpp"

namespace firmware {

// since this is a single class some functions and static data are defined in the cpp
// file to prevent pulling ESP32 OTA related headers in the hpp file

static esp_err_t clientInitCallback(esp_http_client_handle_t client);
static bool errorCheck(esp_err_t esp_rc, const char *details);
static bool isSameImage(const esp_app_desc_t *asiis, const esp_app_desc_t *tobe);
static void partitionMarkValid(TimerHandle_t handle);

static const char *TAG = "ota";
static TaskHandle_t _task_handle = nullptr;
static esp_https_ota_handle_t _ota_handle = nullptr;

static constexpr size_t base_url_len = 256;
static char base_url[base_url_len];

OTA::OTA(TaskHandle_t notify_task, const char *file, const char *ca_start)
    : _notify_task(notify_task), _ca_start(ca_start) {

  auto *p = (char *)memccpy(_url, base_url, 0x00, base_url_len);
  p--; // back up one, memccpy returns pointer to address after copied null

  // ensure there is a slash separator
  if (*(p - 1) != '/') *p++ = '/';

  // copy the firmware file to fetch into the url
  memccpy(p, file, 0x00, (p - _url) - _url_max_len);

  ESP_LOGI(TAG, "url='%s'", _url);

  start();
}

OTA::~OTA() {
  while (_task_handle) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void OTA::captureBaseUrl(const char *url) { memccpy(base_url, url, 0x00, base_url_len); }

OTA::Notifies OTA::core() {

  const esp_partition_t *ota_part = esp_ota_get_next_update_partition(nullptr);
  esp_https_ota_config_t ota_config = {};
  esp_http_client_config_t http_conf = {};
  http_conf.url = _url;
  http_conf.cert_pem = _ca_start;
  http_conf.keep_alive_enable = true;
  http_conf.timeout_ms = 1000;

  ota_config.http_config = &http_conf;
  ota_config.http_client_init_cb = clientInitCallback;
  ota_config.partial_http_download = true;

  // track the time it takes to perform ota
  _start_at = esp_timer_get_time();

  auto esp_rc = esp_https_ota_begin(&ota_config, &_ota_handle);
  if (errorCheck(esp_rc, "(ota begin)")) return Notifies::ERROR;

  const esp_app_desc_t *app_curr = esp_ota_get_app_description();
  esp_app_desc_t app_new;
  auto img_rc = esp_https_ota_get_img_desc(_ota_handle, &app_new);

  if (errorCheck(img_rc, "(get img desc)")) return Notifies::ERROR;
  if (isSameImage(app_curr, &app_new)) return Notifies::CANCEL;

  ESP_LOGI(TAG, "begin partition=\"%s\" addr=0x%x", ota_part->label, ota_part->address);

  do {
    esp_rc = esp_https_ota_perform(_ota_handle);
  } while (esp_rc == ESP_ERR_HTTPS_OTA_IN_PROGRESS);

  auto ota_finish_rc = esp_https_ota_finish(_ota_handle);
  _ota_handle = nullptr;

  // give priority to errors from ota_begin() vs. ota_finish()
  const auto ota_rc = (esp_rc != ESP_OK) ? esp_rc : ota_finish_rc;
  if (errorCheck(ota_rc, "(perform or finish)")) return Notifies::ERROR;

  _elapsed_ms = (esp_timer_get_time() - _start_at) / 1000;
  ESP_LOGI(TAG, "finished in %ums", _elapsed_ms);

  return Notifies::FINISH;
}

void OTA::coreTask(void *task_data) {
  OTA *ota = (OTA *)task_data;

  ota->notifyParent(START);

  auto notify_val = ota->core();

  ota->notifyParent(notify_val);

  // ensure the esp_https_ota context is freed
  if (_ota_handle) {
    esp_https_ota_finish(_ota_handle);
    _ota_handle = nullptr;
  }

  TaskHandle_t task = _task_handle;
  _task_handle = nullptr;

  ESP_LOGD("OTA", "task ending...");
  vTaskDelete(task); // remove task from scheduler
}

void OTA::start() {
  // ignore requets if the task is already running
  if (_task_handle != nullptr) return;

  // this (object) is passed as the data to the task creation and is
  // used by the static runEngine method to call the run method
  xTaskCreate(&coreTask, "ota", 5120, this, 1, &_task_handle);
}

void OTA::handlePendingIfNeeded(const uint32_t valid_ms) {
  const esp_partition_t *run_part = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;

  if (esp_ota_get_state_partition(run_part, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {

      auto timer =
          xTimerCreate("ota_validate", pdMS_TO_TICKS(valid_ms), pdFALSE, nullptr, &partitionMarkValid);

      ESP_LOGI(TAG, "found pending partition, starting validate timer");

      xTimerStart(timer, pdMS_TO_TICKS(0));
    }
  }
}

void OTA::notifyParent(OTA::Notifies notify_val) {
  xTaskNotify(_notify_task, notify_val, eSetValueWithOverwrite);
}

//
// STATIC
//

esp_err_t clientInitCallback(esp_http_client_handle_t client) { return ESP_OK; }

bool errorCheck(esp_err_t esp_rc, const char *details) {
  if (esp_rc != ESP_OK) {
    ESP_LOGE(TAG, "%s %s %s", __PRETTY_FUNCTION__, esp_err_to_name(esp_rc), details);

    return true;
  }

  return false;
}

bool isSameImage(const esp_app_desc_t *asis, const esp_app_desc_t *tobe) {
  auto constexpr SAME = "is already installed";
  auto constexpr DIFF = "will be installed";

  const size_t bytes = sizeof(asis->app_elf_sha256);
  const uint8_t *sha1 = asis->app_elf_sha256;
  const uint8_t *sha2 = tobe->app_elf_sha256;

  auto rc = false;
  if (memcmp(sha1, sha2, bytes) == 0) rc = true;

  ESP_LOGI(TAG, "image version='%s' %s", tobe->version, (rc) ? SAME : DIFF);

  return rc;
}

void partitionMarkValid(TimerHandle_t handle) {
  const esp_partition_t *run_part = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;

  if (esp_ota_get_state_partition(run_part, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_err_t mark_valid_rc = esp_ota_mark_app_valid_cancel_rollback();

      if (mark_valid_rc == ESP_OK) {
        ESP_LOGI(TAG, "partition=\"%s\" marked as valid", run_part->label);
      } else {
        ESP_LOGW(TAG, "[%s] failed to mark partition=\"%s\" as valid", esp_err_to_name(mark_valid_rc),
                 run_part->label);
      }
    }
  } else {
    ESP_LOGE(TAG, "%s failed to get state of run_part=%p", __PRETTY_FUNCTION__, run_part);
  }

  xTimerDelete(handle, 0);
}

} // namespace firmware
