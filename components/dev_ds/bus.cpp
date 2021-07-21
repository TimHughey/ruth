/*
    Ruth
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

#include <cstring>

#include <esp_attr.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "bus.hpp"
#include "owb/owb.h"

namespace ds {

static const char *TAG = "ds:bus";

static TaskHandle_t bus_holder = nullptr;
static SemaphoreHandle_t mutex = nullptr;
static OneWireBus *owb = nullptr;
static uint8_t powered = false;

static owb_status status = OWB_STATUS_OK;
static bool present = false;

static inline bool ok() { return status == OWB_STATUS_OK; }

bool Bus::acquire(uint32_t timeout_ms) {

  auto rc = false;
  const UBaseType_t wait_ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

  auto bus_requestor = xTaskGetCurrentTaskHandle();

  if (bus_requestor == bus_holder) {
    return true;
  }

  auto take_rc = xSemaphoreTake(mutex, wait_ticks);

  if (take_rc == pdTRUE) {
    bus_holder = bus_requestor;
    ESP_LOGD(TAG, "ACQUIRE bus holder: %p", bus_holder);
    rc = true;
  } else {
    ESP_LOGW(TAG, "semaphore take failed: %d", take_rc);
  }

  return rc;
}

bool Bus::ensure() {
  static owb_rmt_driver_info rmt_driver;
  constexpr uint8_t pin = 14;
  owb = owb_rmt_initialize(&rmt_driver, pin, RMT_CHANNEL_0, RMT_CHANNEL_1);

  if (owb) {
    owb_use_crc(owb, true);

    mutex = xSemaphoreCreateMutex();
    xSemaphoreGive(mutex);
    // checkPowered();
    return true;
  }

  ESP_LOGW(TAG, "owb rmt initialization failed");
  return false;
}

IRAM_ATTR bool Bus::error() {
  auto const rc = status != OWB_STATUS_OK;

  if (rc == true) {
    ESP_LOGW(TAG, "error: %d", status);
  }

  return rc;
}

IRAM_ATTR void Bus::checkPowered() {
  static const uint8_t read_powered_cmd[] = {0xcc, 0xb4};
  static constexpr size_t len = sizeof(read_powered_cmd);

  if (reset()) {
    status = owb_write_bytes(owb, read_powered_cmd, len);

    if (ok()) {
      status = owb_read_byte(owb, &powered);
    }
  }
}

IRAM_ATTR bool Bus::convert(bool &complete, bool cancel) {
  static uint8_t convert_status = 0;
  static bool in_progress = false;

  if (cancel) {
    reset();
    in_progress = false;
    complete = true;
    return true;
  }

  // the in-progress code path is executed multiple times so it must be the most efficient
  if (in_progress) {
    // NOTE:  do not reset the bus while the convert is in progress
    status = owb_read_byte(owb, &convert_status);

    if (error()) {
      convert(complete, true); // call ourself with cancel = true
      return false;            // signal error
    }

    // devices hold the bus low while converting; a non-zero indicates convert complete
    if (convert_status) {
      return convert(complete, true); // call ourself to reset the convert
    }

    complete = false;
    return true;
  }

  // the start code path is executed once per convert
  // convert not started, let's start it
  static uint8_t convert_cmd[] = {0xcc, 0x44};
  static constexpr size_t len = sizeof(convert_cmd);

  if (reset()) {
    status = owb_write_bytes(owb, convert_cmd, len);

    if (error()) {
      convert(complete, true);
      return false;
    }

    complete = false;
    in_progress = true;
    return true;
  }

  // the reset before starting the convert failed
  convert(complete, true); // call ourself to reset
  return false;
}

IRAM_ATTR uint8_t Bus::lastStatus() { return status; }

IRAM_ATTR bool Bus::ok() { return status == OWB_STATUS_OK; }

bool Bus::release() {
  auto task = xTaskGetCurrentTaskHandle();

  if (task != bus_holder) {
    return false;
  }

  auto rc = false;

  ESP_LOGD(TAG, "RELEASE bus holder %p", bus_holder);
  bus_holder = nullptr;
  auto give_rc = xSemaphoreGive(mutex);

  if (give_rc == pdTRUE) {
    rc = true;
  } else {
    ESP_LOGW(TAG, "semaphore give failed: %d", give_rc);
  }

  return rc;
}

IRAM_ATTR bool Bus::reset() {
  status = owb_reset(owb, &present);
  if (status == OWB_STATUS_OK) return true;

  ESP_LOGW(TAG, "reset failed: [%d]", status);
  return false;
}

static OneWireBus_SearchState state = {};
IRAM_ATTR bool Bus::search(uint8_t *rom_code) {
  static bool in_progress = false;

  bool found = false;

  if (reset()) {
    if (in_progress) {
      status = owb_search_next(owb, &state, &found);
    } else {
      status = owb_search_first(owb, &state, &found);
    }

    if (ok() && found) {
      // a rom code was found, copy it to the caller and flag that a search
      // is in progress.
      constexpr size_t len = sizeof(state.rom_code.bytes);
      memcpy(rom_code, state.rom_code.bytes, len);
      in_progress = true;
      return true;
    }
  }

  // rom code not found, end of available devices or reset failed
  in_progress = false;

  return false;
}

IRAM_ATTR bool Bus::writeThenRead(Bytes write, Len wlen, Bytes read, Len rlen) {
  auto rc = false;

  if (reset()) {
    status = owb_write_bytes(owb, write, wlen);

    if (ok()) {
      status = owb_read_bytes(owb, read, rlen);
    }

    rc = true;
  }

  // always reset the bus after a read / write as some cmd sequences
  // place devices in continous stream mode (e.g. channel access read)
  reset();

  return rc;
}

} // namespace ds
