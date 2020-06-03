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
#include "external/ArduinoJson.h"
#include "misc/restart.hpp"

namespace ruth {

static const char *TAG = "OTATask";

static OTA_t *__singleton__ = nullptr;

OTA_t *OTA::_instance_(MsgPayload_t_ptr payload_ptr) {
  if (payload_ptr.get()) {
    __singleton__ = new OTA(move(payload_ptr));
  }

  return __singleton__;
}

// document examples:
// OTA:
//  {"cmd":"ota","mtime":1589852135,
//   "uri":"https://www.wisslanding.com/helen/firmware/latest.bin"}
//
// RESTART:
//  {"cmd":"restart","mtime":1589852135}
//
static const size_t _capacity = JSON_OBJECT_SIZE(3) + 336;

OTA::OTA(MsgPayload_t_ptr payload_ptr) {
  DynamicJsonDocument doc(_capacity);

  elapsedMicros parse_elapsed;
  // deserialize the msgpack data
  DeserializationError err =
      deserializeMsgPack(doc, payload_ptr.get()->payload());

  // we're done with the original payload at this point
  payload_ptr.reset();

  // parsing complete, freeze the elapsed timer
  parse_elapsed.freeze();

  // did the deserailization succeed?
  if (err) {
    ESP_LOGW(TAG, "[%s] MSGPACK parse failure", err.c_str());
    return;
  }

  _uri = doc["uri"] | "";
}

void OTA::process() {
  const esp_partition_t *ota_part = esp_ota_get_next_update_partition(nullptr);
  esp_http_client_config_t config = {};
  config.url = _uri.c_str();
  config.cert_pem = Net::ca_start();
  config.event_handler = OTA::httpEventHandler;
  config.timeout_ms = 1000;

  textReading::rlog("OTA begin partition=\"%s\" addr=0x%x", ota_part->label,
                    ota_part->address);

  // track the time it takes to perform ota
  elapsedMicros ota_elapsed;
  esp_err_t esp_rc = esp_https_ota(&config);

  textReading::rlog("OTA elapsed %0.2fs [%s]", (float)ota_elapsed,
                    esp_err_to_name(esp_rc));

  Restart::restart("OTA complete");
}

// STATIC
void OTA::markPartitionValid() {
  static bool ota_marked_valid = false; // only do this once

  if (OTA::inProgress() || (ota_marked_valid == true)) {
    return;
  }

  const esp_partition_t *run_part = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(run_part, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_err_t mark_valid_rc = esp_ota_mark_app_valid_cancel_rollback();

      if (mark_valid_rc == ESP_OK) {
        textReading::rlog("[%s] partition=\"%s\" marked as valid",
                          esp_err_to_name(mark_valid_rc), run_part->label);
      } else {
        textReading::rlog("[%s] failed to mark partition=\"%s\" as valid",
                          esp_err_to_name(mark_valid_rc), run_part->label);
      }
    }
  }

  ota_marked_valid = true;
}

const unique_ptr<char[]> OTA::debug() {
  const size_t max_buf = 256;
  unique_ptr<char[]> debug_str(new char[max_buf]);
  snprintf(debug_str.get(), max_buf, "OTA(uri=\"%s\")", _uri.c_str());

  return move(debug_str);
}

//
// STATIC!
//
esp_err_t OTA::httpEventHandler(esp_http_client_event_t *evt) {
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
