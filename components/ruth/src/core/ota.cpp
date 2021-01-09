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
using namespace reading;

static OTA_t __singleton__;

// document examples:
// OTA:
//  {"cmd":"ota","mtime":1589852135,
//   "uri":"https://www.wisslanding.com/helen/firmware/latest.bin"}
//
// RESTART:
//  {"cmd":"restart","mtime":1589852135}
//

void OTA::_handlePendPartIfNeeded_() {
  const esp_partition_t *run_part = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;

  if (esp_ota_get_state_partition(run_part, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      _valid_timer = xTimerCreate("ota_validate", pdMS_TO_TICKS(_ota_valid_ms),
                                  pdFALSE, nullptr, &markPartitionValid);
      vTimerSetTimerID(_valid_timer, this);
      xTimerStart(_valid_timer, pdMS_TO_TICKS(0));
    }
  }
}

bool OTA::queuePayload(MsgPayload_t_ptr payload_ptr) {
  const size_t _capacity = JSON_OBJECT_SIZE(3) + 336;
  // ok to use a DynamicJsonDocument here as we're not generally concerned
  // about long term heap fragmentation
  StaticJsonDocument<_capacity> doc;

  elapsedMicros parse_elapsed;
  // deserialize the msgpack data
  DeserializationError err =
      deserializeMsgPack(doc, payload_ptr.get()->payload());

  // parsing complete, freeze the elapsed timer
  parse_elapsed.freeze();

  // did the deserailization succeed?
  if (!err) {
    _instance_()->_uri = doc["uri"] | "";
    start();
    return true;
  }

  return false;
}

bool OTA::queuePayload(const char *payload) {
  const size_t _capacity = JSON_OBJECT_SIZE(3) + 336;
  // ok to use a DynamicJsonDocument here as we're not generally concerned
  // about long term heap fragmentation
  StaticJsonDocument<_capacity> doc;

  elapsedMicros parse_elapsed;
  // deserialize the msgpack data
  DeserializationError err = deserializeMsgPack(doc, payload);

  // parsing complete, freeze the elapsed timer
  parse_elapsed.freeze();

  // did the deserailization succeed?
  if (!err) {
    _instance_()->_uri = doc["uri"] | "";
    start();
    return true;
  }

  return false;
}

void OTA::process() {
  const esp_partition_t *ota_part = esp_ota_get_next_update_partition(nullptr);
  esp_http_client_config_t config = {};
  config.url = _uri.c_str();
  config.cert_pem = Net::ca_start();
  config.event_handler = OTA::httpEventHandler;
  config.timeout_ms = 1000;

  Text::rlog("OTA begin partition=\"%s\" addr=0x%x", ota_part->label,
             ota_part->address);

  // track the time it takes to perform ota
  elapsedMicros ota_elapsed;
  esp_err_t esp_rc = esp_https_ota(&config);

  Text::rlog("OTA elapsed %0.2fs [%s]", (float)ota_elapsed,
             esp_err_to_name(esp_rc));

  Restart("OTA complete");
}

// STATIC
void OTA::markPartitionValid(TimerHandle_t handle) {
  const esp_partition_t *run_part = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  OTA_t *ota = _instance_();

  if (esp_ota_get_state_partition(run_part, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_err_t mark_valid_rc = esp_ota_mark_app_valid_cancel_rollback();

      if (mark_valid_rc == ESP_OK) {
        Text::rlog("[%s] partition=\"%s\" marked as valid",
                   esp_err_to_name(mark_valid_rc), run_part->label);
      } else {
        Text::rlog("[%s] failed to mark partition=\"%s\" as valid",
                   esp_err_to_name(mark_valid_rc), run_part->label);
      }
    }
  }

  ota->_ota_marked_valid = true;

  if (handle) {
    xTimerDelete(handle, 0);
  }
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

OTA_t *OTA::_instance_() { return &__singleton__; }
} // namespace ruth
