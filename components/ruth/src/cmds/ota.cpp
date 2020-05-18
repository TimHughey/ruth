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

#include "cmds/ota.hpp"
#include "misc/restart.hpp"

namespace ruth {

static const char *TAG = "CmdOTA";
static bool _ota_in_progress = false;

CmdOTA::CmdOTA(JsonDocument &doc, elapsedMicros &e) : Cmd(doc, e) {
  _uri = doc["uri"] | "none.local";
}

void CmdOTA::doUpdate() {
  const esp_partition_t *run_part = esp_ota_get_running_partition();
  esp_http_client_config_t config = {};
  config.url = _uri.c_str();
  config.cert_pem = Net::ca_start();
  config.event_handler = CmdOTA::httpEventHandler;
  config.timeout_ms = 1000;

  if (_ota_in_progress) {
    ESP_LOGW(TAG, "ota in-progress, ignoring spurious begin");
    return;
  } else {
    textReading_t *rlog = new textReading_t;
    textReading_ptr_t rlog_ptr(rlog);

    rlog->printf("OTA begin part(run) name(%-8s) addr(0x%x)", run_part->label,
                 run_part->address);
    rlog->publish();
    ESP_LOGI(TAG, "%s", rlog->text());
  }

  _ota_in_progress = true;

  textReading_t *rlog = new textReading_t;
  textReading_ptr_t rlog_ptr(rlog);

  // track the time it takes to perform ota
  elapsedMicros ota_elapsed;
  esp_err_t esp_rc = esp_https_ota(&config);

  rlog->printf("[%s] OTA elapsed(%0.2fs)", esp_err_to_name(esp_rc),
               ota_elapsed.asSeconds());

  if (esp_rc == ESP_OK) {
    ESP_LOGI(TAG, "%s", rlog->text());

  } else {
    ESP_LOGE(TAG, "%s", rlog->text());
  }

  Restart::restart(rlog->text(), __PRETTY_FUNCTION__);
}

// STATIC
void CmdOTA::markPartitionValid() {
  static bool ota_marked_valid = false; // only do this once

  if (ota_marked_valid == true) {
    return;
  }

  const esp_partition_t *run_part = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(run_part, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_err_t mark_valid_rc = esp_ota_mark_app_valid_cancel_rollback();

      textReading_t *rlog = new textReading_t;
      textReading_ptr_t rlog_ptr(rlog);

      if (mark_valid_rc == ESP_OK) {
        rlog->printf("[%s] partition [%s] marked as valid",
                     esp_err_to_name(mark_valid_rc), run_part->label);
        rlog->publish();
        ESP_LOGI(TAG, "%s", rlog->text());
      } else {
        rlog->printf("[%s] failed to mark partition [%s] as valid",
                     esp_err_to_name(mark_valid_rc), run_part->label);
        rlog->publish();
        ESP_LOGE(TAG, "%s", rlog->text());
      }
    }
  }

  ota_marked_valid = true;
}

bool CmdOTA::process() {

  if (forThisHost() == false) {
    ESP_LOGD(TAG, "OTA command not for us, ignoring.");
    return true;
  }

  switch (type()) {

  case CmdType::otaHTTPS:
    ESP_LOGI(TAG, "OTA via HTTPS requested");
    doUpdate();
    break;

  case CmdType::restart:
    Restart::restart("restart requested", __PRETTY_FUNCTION__);
    break;

  default:
    ESP_LOGW(TAG, "unknown ota command, ignoring");
    break;
  };

  return true;
}

const unique_ptr<char[]> CmdOTA::debug() {
  const size_t max_buf = 256;
  unique_ptr<char[]> debug_str(new char[max_buf]);
  snprintf(debug_str.get(), max_buf, "Cmd(host=\"%s\" uri=\"%s\"",
           host().c_str(), _uri.c_str());

  return move(debug_str);
}

//
// STATIC!
//
esp_err_t CmdOTA::httpEventHandler(esp_http_client_event_t *evt) {
  switch (evt->event_id) {
  case HTTP_EVENT_ERROR:
    // ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
    break;
  case HTTP_EVENT_ON_CONNECTED:
    // ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
    break;
  case HTTP_EVENT_HEADER_SENT:
    // ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
    break;
  case HTTP_EVENT_ON_HEADER:
    ESP_LOGI(TAG, "OTA HTTPS HEADER: key(%s), value(%s)", evt->header_key,
             evt->header_value);
    break;
  case HTTP_EVENT_ON_DATA:
    // ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
    break;
  case HTTP_EVENT_ON_FINISH:
    // ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
    break;
  case HTTP_EVENT_DISCONNECTED:
    // ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
    break;
  }
  return ESP_OK;
}
} // namespace ruth
