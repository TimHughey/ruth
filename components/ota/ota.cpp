//  Ruth
//  Copyright (C) 2020  Tim Hughey
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

#include "ota/ota.hpp"

#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <spi_flash_mmap.h>
#include <string>

namespace ruth {

namespace firmware {

// since this is a single class some functions and static data are defined in the cpp
// file to prevent pulling ESP32 OTA related headers in the hpp file

static esp_err_t clientInitCallback(esp_http_client_handle_t client) noexcept { return ESP_OK; }

static bool errorCheck(esp_err_t esp_rc, const char *details) noexcept {
  if (esp_rc != ESP_OK) {
    ESP_LOGE(OTA::TAG, "%s %s %s", __PRETTY_FUNCTION__, esp_err_to_name(esp_rc), details);

    return true;
  }

  return false;
}

static bool is_same_image(const esp_app_desc_t *a, esp_app_desc_t *b) noexcept {
  auto constexpr SAME{"is already installed"};
  auto constexpr DIFF{"will be installed"};

  auto rc = memcmp(a->app_elf_sha256, b->app_elf_sha256, sizeof(a->app_elf_sha256)) == 0;

  ESP_LOGI(OTA::TAG, "image %s %s %s %s", b->version, b->date, b->time, rc ? SAME : DIFF);

  return rc;
}

static void partition_mark_valid(TimerHandle_t handle) {
  const esp_partition_t *run_part = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;

  if (esp_ota_get_state_partition(run_part, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_err_t mark_valid_rc = esp_ota_mark_app_valid_cancel_rollback();

      if (mark_valid_rc == ESP_OK) {
        ESP_LOGI(OTA::TAG, "partition=\"%s\" marked as valid", run_part->label);
      } else {
        ESP_LOGW(OTA::TAG, "[%s] failed to mark partition=\"%s\" as valid",
                 esp_err_to_name(mark_valid_rc), run_part->label);
      }
    }
  } else {
    ESP_LOGE(OTA::TAG, "%s failed to get state of run_part=%p", __PRETTY_FUNCTION__, run_part);
  }

  xTimerDelete(handle, 0);
}

OTA::OTA(const char *base_url, const char *file,
         const char *ca_start) noexcept
    : notify_task(xTaskGetCurrentTaskHandle()), //
      ca_start(ca_start),                       //
      url(base_url),                            // prep url with the base_url
      self_task{nullptr}                        //
{
  if (url.empty() || (csv(url) == csv{"UNSET"})) return; // protect against empty url
  if (!url.ends_with('/')) url.append("/");              // ensure base url includes a slash

  url.append(file); // add the file

  if (url.size() > URL_MAX_LEN) url.clear(); // protect against oversize urls

  if (url.empty()) { // only start OTA if url is valid
    notifyParent(OTA::ERROR);
  } else {
    // this (object) is passed as the data to the task creation and is
    // used by the static runEngine method to call the run method
    xTaskCreate(&coreTask, "ota", 5120, this, 3, &self_task);
  }
}

OTA::~OTA() noexcept {
  while (self_task) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

OTA::Notifies OTA::core() {

  const esp_partition_t *ota_part = esp_ota_get_next_update_partition(nullptr);
  esp_https_ota_config_t ota_config{};
  esp_http_client_config_t http_conf{};
  http_conf.url = url.c_str();
  http_conf.cert_pem = ca_start;
  http_conf.keep_alive_enable = true;
  http_conf.timeout_ms = 1000;

  ota_config.http_config = &http_conf;
  ota_config.http_client_init_cb = clientInitCallback;
  ota_config.partial_http_download = true;

  // track the time it takes to perform ota
  e.reset();

  esp_https_ota_handle_t handle;
  auto esp_rc = esp_https_ota_begin(&ota_config, &handle);

  ota_handle.emplace(handle); // save the handle in case of errors

  if (errorCheck(esp_rc, "(ota begin)")) return Notifies::ERROR;

  const esp_app_desc_t *app_curr = esp_app_get_description();
  esp_app_desc_t app_new;
  auto img_rc = esp_https_ota_get_img_desc(handle, &app_new);

  if (errorCheck(img_rc, "(get img desc)")) return Notifies::ERROR;

  if (is_same_image(app_curr, &app_new)) return Notifies::CANCEL;

  ESP_LOGI(TAG, "begin partition=\"%s\" addr=0x%lx", ota_part->label, ota_part->address);

  do {
    esp_rc = esp_https_ota_perform(handle);
  } while (esp_rc == ESP_ERR_HTTPS_OTA_IN_PROGRESS);

  auto ota_finish_rc = esp_https_ota_finish(handle);
  ota_handle.reset(); // no clean up of ota handle required

  if (errorCheck(ota_finish_rc, "(perform or finish)")) return Notifies::ERROR;

  ESP_LOGI(TAG, "finished in %lldms", e.as<Millis>().count());

  return Notifies::FINISH;
}

void OTA::coreTask(void *task_data) {
  auto *ota = static_cast<OTA *>(task_data);

  ota->notifyParent(START);

  auto notify_val = ota->core();

  ota->notifyParent(notify_val);

  // ensure the esp_https_ota context is freed
  if (ota->ota_handle.has_value()) {
    esp_https_ota_finish(ota->ota_handle.value());

    ota->ota_handle.reset();
  }

  ESP_LOGI("OTA", "task ending...");

  TaskHandle_t task{nullptr};
  std::swap(task, ota->self_task);

  vTaskDelete(task); // remove task from scheduler
}

void OTA::handlePendingIfNeeded(const uint32_t valid_ms) {
  const esp_partition_t *run_part = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;

  if (esp_ota_get_state_partition(run_part, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {

      auto timer = xTimerCreate(   //
          "ota_validate",          //
          pdMS_TO_TICKS(valid_ms), //
          pdFALSE,                 // no auto reload
          nullptr,                 // no user context
          &partition_mark_valid    //
      );

      ESP_LOGI(TAG, "found pending partition, starting validate timer");

      xTimerStart(timer, pdMS_TO_TICKS(0));
    }
  }
}

void OTA::notifyParent(OTA::Notifies notify_val) {
  xTaskNotify(notify_task, notify_val, eSetValueWithOverwrite);
}

//
// STATIC
//

} // namespace firmware
} // namespace ruth
