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

#include "desk_cmd/ota.hpp"
#include "ArduinoJson.h"
#include "async_msg/write.hpp"
#include "desk_msg/out.hpp"
#include "desk_msg/out_info.hpp"

#include <algorithm>
#include <array>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <iterator>
#include <spi_flash_mmap.h>

extern const char ca_start[] asm("_binary_ca_pem_start");
extern const char ca_end[] asm("_binary_ca_pem_end");
static const auto ca_len{std::distance(ca_start, ca_end)};

namespace ruth {
namespace desk {

static esp_err_t clientInitCallback(esp_http_client_handle_t client) noexcept { return ESP_OK; }

OTA::OTA(tcp_socket &&sock, const char *base_url, const char *file) noexcept
    : sock(std::move(sock)), url(base_url), state{INIT} {

  if (!url.empty()) {
    if (!url.ends_with('/')) url.append("/"); // ensure base url includes a slash

    url.append(file); // add the file
  }

  if (url.empty() || (url.size() >= URL_MAX_LEN)) {
    error.assign("url error");
    state = ERROR;
  }
}

OTA::~OTA() noexcept {
  [[maybe_unused]] error_code ec;

  sock.shutdown(tcp_socket::shutdown_both, ec);
  sock.close(ec);
}

bool OTA::check_error(esp_err_t esp_rc, const char *details) noexcept {
  if (esp_rc != ESP_OK) {
    if (error.empty()) {
      error.append(details).append(" ").append(esp_err_to_name(esp_rc));
    }
    return true;
  }

  return false;
}

void OTA::download() noexcept {
  auto esp_rc = esp_https_ota_perform(ota_handle);

  if (esp_rc == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
    asio::post(sock.get_executor(), [s = shared_from_this()]() mutable { s->download(); });
  } else {
    state = check_error(esp_rc, "(perform or finish)") ? ERROR : FINISH;

    finish();
  }
}
void OTA::execute() noexcept {

  if (state != INIT) finish();

  // prevent log noise
  esp_log_level_set("HTTP_CLIENT", ESP_LOG_ERROR);
  esp_log_level_set("esp_https_ota", ESP_LOG_ERROR);

  asio::post(sock.get_executor(), [self_s = shared_from_this()]() {
    auto self = self_s.get(); // keep ourself
    auto &state = self->state;

    state = self->initialize();

    if (state == EXECUTE) {
      self->download();
    } else {
      self->finish();
    }
  });
}

void OTA::finish() noexcept {
  if (ota_handle) {
    auto rc = esp_https_ota_finish(std::exchange(ota_handle, nullptr));
    check_error(rc, "ota finish");
  }

  auto constexpr INSTALLED{" installed"};

  string result(version);

  if (state == CANCEL) {
    result.append(" is").append(INSTALLED);
  } else if (state == FINISH) {
    result.append(INSTALLED);
  } else {
    auto constexpr SEE_ERROR{"error"};
    if (!result.empty()) {
      result.append(" (see ").append(SEE_ERROR).append(")");
    } else {
      result.assign(SEE_ERROR);
    }
  }

  ESP_LOGI(TAG, "%s %s", result.c_str(), error.c_str());

  MsgOutWithInfo msg(desk::OTA_RESPONSE);

  msg.add_kv(desk::RESULT, result);
  msg.add_kv(desk::ELAPSED_US, e());

  if (!error.empty()) msg.add_kv(desk::ERROR, error);

  send_response(std::move(msg));
}

OTA::state_t OTA::initialize() noexcept {
  // setup the overall context for OTA
  const auto ota_part{esp_ota_get_next_update_partition(nullptr)};

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

  auto esp_rc = esp_https_ota_begin(&ota_config, &ota_handle);

  if (check_error(esp_rc, "(ota begin)")) return ERROR;

  const esp_app_desc_t *app_curr = esp_app_get_description();
  esp_app_desc_t app_new;
  auto img_rc = esp_https_ota_get_img_desc(ota_handle, &app_new);

  if (check_error(img_rc, "(get img desc)")) return ERROR;

  if (is_same_image(app_curr, &app_new)) return CANCEL;

  ESP_LOGI(TAG, "begin partition=\"%s\" addr=0x%lx", ota_part->label, ota_part->address);

  return EXECUTE;
}

bool OTA::is_same_image(const esp_app_desc_t *a, esp_app_desc_t *b) noexcept {
  version.assign(b->version);

  return memcmp(a->app_elf_sha256, b->app_elf_sha256, sizeof(a->app_elf_sha256)) == 0;
}

void OTA::mark_valid(TimerHandle_t handle) noexcept {
  const esp_partition_t *run_part = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;

  if (esp_ota_get_state_partition(run_part, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_err_t mark_valid_rc = esp_ota_mark_app_valid_cancel_rollback();

      if (mark_valid_rc == ESP_OK) {
        ESP_LOGI(OTA::TAG, "partition=%s marked valid", run_part->label);
      } else {
        ESP_LOGW(OTA::TAG, "[%s] failed to mark partition=\"%s\" as valid",
                 esp_err_to_name(mark_valid_rc), run_part->label);
      }
    }
  } else {
    ESP_LOGE(OTA::TAG, "%s failed to get state of run_part=%p", __PRETTY_FUNCTION__, run_part);
  }

  xTimerDelete(handle, pdMS_TO_TICKS(60000));
}

void OTA::validate_pending(Binder *binder) {
  const esp_partition_t *run_part = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;

  if (esp_ota_get_state_partition(run_part, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {

      JsonObjectConst obj = binder->doc_at_key("ota");

      auto timer = xTimerCreate(                        //
          "ota_validate",                               //
          pdMS_TO_TICKS(obj["valid_after_ms"] | 60000), //
          pdFALSE,                                      // no auto reload
          nullptr,                                      // no user context
          &mark_valid                                   //
      );

      ESP_LOGI(TAG, "found pending partition, starting validate timer");

      xTimerStart(timer, pdMS_TO_TICKS(0));
    }
  }
}

void IRAM_ATTR OTA::send_response(MsgOut &&msg) noexcept {

  async_msg::write(sock, std::move(msg), [self = shared_from_this()](MsgOut msg_out) {
    if (msg_out.xfer_error()) {
      ESP_LOGI(TAG, "write reply error %s", msg_out.ec.message().c_str());
    }

    if (self->state == FINISH) {
      auto timer = xTimerCreate("ota_restart", pdMS_TO_TICKS(1000),
                                pdFALSE, // no auto reload
                                nullptr, // no user context
                                &restart);

      ESP_LOGI(TAG, "restart timer=%p", timer);

      xTimerStart(timer, pdMS_TO_TICKS(1000));
    }
  });
}

} // namespace desk
} // namespace ruth
