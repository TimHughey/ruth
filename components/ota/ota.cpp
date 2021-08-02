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
static bool isNewImage(const esp_app_desc_t *asis, const esp_app_desc_t *tobe);
static void partitionMarkValid(TimerHandle_t handle);

static const char *TAG = "ota";
static TaskHandle_t _task_handle = nullptr;
static esp_https_ota_handle_t _ota_handle = nullptr;
static constexpr size_t _base_url_len = 256;
static char _base_url[_base_url_len];

OTA::OTA(TaskHandle_t notify_task, const char *file, const char *ca_start)
    : _notify_task(notify_task), _ca_start(ca_start) {
  auto *p = (char *)memccpy(_url, _base_url, 0x00, _url_max_len);
  p--; // back up one, memccpy returns pointer to address after copied null

  // ensure there is a slash separator
  if (*(p - 1) != '/') {
    *p++ = '/';
  }

  memccpy(p, file, 0x00, (p - _url) - _url_max_len);

  ESP_LOGI(TAG, "url='%s'", _url);

  start();
}

OTA::~OTA() {
  while (_task_handle != nullptr) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void OTA::captureBaseUrl(const char *url) { memccpy(_base_url, url, 0x00, _base_url_len); }

void OTA::core() {
  while (_run_task) {
    _run_task = false;

    Notifies notify_val;
    xTaskNotifyWait(0x00, ULONG_MAX, (uint32_t *)&notify_val, portMAX_DELAY);

    switch (notify_val) {
    case START:
      perform();
      break;

    case CANCEL:
      if (_ota_handle) { // clean up
        ESP_LOGD("OTA", "canceled");
        esp_https_ota_finish(_ota_handle);
        _ota_handle = nullptr;
      }
      xTaskNotify(_notify_task, Notifies::CANCEL, eSetValueWithOverwrite);
      break;

    case FINISH:
      _elapsed_ms = (esp_timer_get_time() - _start_at) / 1000;
      ESP_LOGI(TAG, "finished in %ums, restarting", _elapsed_ms);
      xTaskNotify(_notify_task, Notifies::FINISH, eSetValueWithOverwrite);
      break;

    default:
      break;
    }
  }
}

void OTA::coreTask(void *task_data) {
  OTA *ota = (OTA *)task_data;
  ota->taskNotify(START);
  ota->core();

  TaskHandle_t task = _task_handle;
  _task_handle = nullptr;

  ESP_LOGD("OTA", "task ending...");
  vTaskDelete(task); // remove task from scheduler
}

bool OTA::perform() {
  auto rc = true;

  _run_task = true;

  const esp_partition_t *ota_part = esp_ota_get_next_update_partition(nullptr);
  esp_https_ota_config_t ota_config;
  esp_http_client_config_t http_conf = {};
  http_conf.url = _url;
  http_conf.cert_pem = _ca_start;
  http_conf.keep_alive_enable = true;
  http_conf.timeout_ms = 1000;

  ota_config.http_config = &http_conf;
  ota_config.http_client_init_cb = clientInitCallback;

  // track the time it takes to perform ota
  _start_at = esp_timer_get_time();

  auto esp_rc = esp_https_ota_begin(&ota_config, &_ota_handle);

  if (esp_rc != ESP_OK) {
    ESP_LOGE(TAG, "%s %s", __PRETTY_FUNCTION__, esp_err_to_name(esp_rc));
    return false;
  }

  const esp_app_desc_t *app_curr = esp_ota_get_app_description();
  esp_app_desc_t app_new;
  auto img_rc = esp_https_ota_get_img_desc(_ota_handle, &app_new);

  if (isNewImage(app_curr, &app_new) && (img_rc == ESP_OK)) {
    // this is a new image
    ESP_LOGI(TAG, "begin partition=\"%s\" addr=0x%x", ota_part->label, ota_part->address);

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
      taskNotify(FINISH);
    } else {
      ESP_LOGE(TAG, "[%s] failure", esp_err_to_name(ota_rc));
      rc = false;
    }
  } else if (img_rc == ESP_OK) {
    // is is not a new image, clean up the ota_begin and return success
    taskNotify(CANCEL);
    rc = true;
  } else {
    // failure with initial esp_begin
    rc = false;
  }

  return rc;
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

void OTA::taskNotify(Notifies val) { xTaskNotify(_task_handle, val, eSetValueWithOverwrite); }

esp_err_t clientInitCallback(esp_http_client_handle_t client) { return ESP_OK; }

bool isNewImage(const esp_app_desc_t *asis, const esp_app_desc_t *tobe) {
  const size_t bytes = sizeof(asis->app_elf_sha256);
  const uint8_t *sha1 = asis->app_elf_sha256;
  const uint8_t *sha2 = tobe->app_elf_sha256;

  bool rc = false;

  if (memcmp(sha1, sha2, bytes)) rc = true;

  ESP_LOGI(TAG, "image version='%s' %s", tobe->version, (rc) ? "will be installed" : "is already installed");

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
