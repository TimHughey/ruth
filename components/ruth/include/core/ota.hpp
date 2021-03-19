/*
    ota.hpp - Ruth Core OTA Class
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

#ifndef _ruth_core_ota_hpp
#define _ruth_core_ota_hpp

#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_spi_flash.h>

#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "protocols/payload.hpp"

namespace ruth {

typedef class OTA OTA_t;

class OTA {
  typedef enum {
    NotifyOtaStart = 0x01,
    NotifyOtaCancel,
    NotifyOtaFinish
  } NotifyVal_t;

public:
  OTA() = default;
  ~OTA();

  bool handleCommand(MsgPayload &payload);
  void start();

  static void partitionHandlePendingIfNeeded();
  static void partitionMarkValid(TimerHandle_t handle = nullptr);

private:
  void cancel();
  void core(); // main task loop
  static void coreTask(void *task_data);

  static esp_err_t httpEventHandler(esp_http_client_event_t *evt);

  bool isNewImage(const esp_app_desc_t *asis, const esp_app_desc_t *tobe);

  bool perform();

  inline TaskHandle_t &taskHandle() { return _task.handle; }
  inline void taskNotify(NotifyVal_t val) {
    xTaskNotify(taskHandle(), val, eSetValueWithOverwrite);
  }

private:
  elapsedMicros _elapsed;

  esp_https_ota_handle_t _ota_handle = nullptr;
  MsgPackPayload_t *_payload = nullptr;
  bool _run_task = true;

  Task_t _task = {.handle = nullptr,
                  .data = nullptr,
                  .priority = 1, // allow reporting to continue
                  .stackSize = (5 * 1024)};

  OtaUri_t _uri;
};

} // namespace ruth

#endif
