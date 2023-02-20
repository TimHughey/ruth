//  Ruth
//  Copyright (C) 2021 Tim Hughey
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

#include "core/core.hpp"
#include "ArduinoJson.h"
#include "binder/binder.hpp"
#include "core/boot_msg.hpp"
#include "core/engines.hpp"
#include "core/run_msg.hpp"
#include "core/sntp.hpp"
#include "core/startup_msg.hpp"
#include "dev_pwm/pwm.hpp"
#include "misc/status_led.hpp"
#include "network/network.hpp"
#include "ota/ota.hpp"
#include "ruth_mqtt/mqtt.hpp"

#include <esp_log.h>
#include <memory>
#include <string_view>

namespace ruth {

static const char *TAG{"Core"};

Core::Core() noexcept
    : message::Handler("host", MAX_QUEUE_DEPTH),            //
      heap_first(heap_caps_get_free_size(MALLOC_CAP_8BIT)), //
      heap_avail(heap_first),                               //
      heap_track_ms{5 * 1000},                              //
      engines_started{false},                               //
      report_timer_handle{nullptr},                         //
      ota(nullptr)                                          //
{
  StatusLED::init();
  Binder::init();

  StatusLED::dim();
  auto wifi = Binder::wifi();

  StatusLED::brighter();

  shared::net = std::make_unique<Net>(Net::Opts(wifi["ssid"], wifi["passwd"], 60000));

  StatusLED::brighter();
  start_sntp(); // only returns if SNTP succeeds

  StatusLED::brighter();
  filter::Filter::init(filter::Opts{Binder::env(), net::host_id(), net::hostname()});

  StatusLED::brighter();
  start_mqtt();

  StatusLED::brighter();
  MQTT::send(message::Startup());
  DynamicJsonDocument doc(2048);

  // wait for profile message from MQTT
  auto msg = waitForMessage(3333);

  if (!msg) {
    ESP_LOGE(TAG, "did not receive profile");
    esp_restart();
  }

  msg->unpack(doc);

  // cache boot info
  ota_base_url.assign(doc["ota"]["base_url"] | "UNSET");

  StatusLED::brighter();
  net::hostname(msg->hostnameFromFilter());

  MQTT::send(message::Boot(CONFIG_ESP_MAIN_TASK_STACK_SIZE, doc["meta"]["name"] | "unknown"));

  StatusLED::percent(75);
  firmware::OTA::handlePendingIfNeeded(doc["ota"]["valid_ms"] | 60000);

  // lower our priority to not compete with actual work
  vTaskPrioritySet(nullptr, priority);

  report_timer_handle = xTimerCreate(                 //
      "core_report",                                  //
      pdMS_TO_TICKS(doc["host"]["report_ms"] | 7000), //
      pdTRUE,                                         // auto reload
      nullptr,                                        //
      &report_timer                                   //
  );

  vTimerSetTimerID(report_timer_handle, this);
  xTimerStart(report_timer_handle, pdMS_TO_TICKS(0)); // do the first heap track

  // only start engines if host has been assigned a name and isn't using the
  // host id (default)
  if (net::has_assigned_name()) {

    doc["hostname"] = net::hostname();
    doc["unique_id"] = net::mac_address();
    Engines::start_configured(doc);
  }

  StatusLED::off();
}

void Core::do_ota(message::InWrapped msg) noexcept {
  using namespace firmware;

  if (ota) return; // OTA already in progress, do nothing

  DynamicJsonDocument ota_cmd(1024);

  if (msg->unpack(ota_cmd)) {
    ota = std::make_unique<firmware::OTA>( //
        ota_base_url.c_str(),              //
        ota_cmd["file"] | "latest.bin",    //
        net::ca_begin()                    //
    );

    while (ota) {

      StatusLED::bright();

      OTA::Notifies val;

      auto constexpr timeout = pdMS_TO_TICKS(1000);
      auto rc = xTaskNotifyWait(0x00, ULONG_MAX, (uint32_t *)&val, timeout);

      StatusLED::dim();

      track_heap();

      if (rc == pdFAIL) continue; // timeout == OTA in progress, just track heap

      switch (val) {
      case OTA::Notifies::START: // ota started, wait for next notify
        continue;

      case OTA::Notifies::FINISH: // finished, restart
        esp_restart();
        break;

      default: // error or cancel
        ota.reset();
      }
    }

    StatusLED::off();
  }
}

void Core::loop() noexcept {

  auto msg = waitForMessage();

  if (msg) {
    switch (msg->kind()) {
    case DocKinds::RESTART:
      esp_restart();
      break;

    case DocKinds::OTA:
      do_ota(std::move(msg));
      break;

    case DocKinds::BINDER:
      ESP_LOGI(TAG, "binder messages");
      break;

    case DocKinds::PROFILE:
      break;
    }
  }
}

void Core::report_timer(TimerHandle_t handle) noexcept {
  static_cast<Core *>(pvTimerGetTimerID(handle))->track_heap();
}

void Core::start_mqtt() noexcept {
  const auto mqtt_cfg = Binder::mqtt();

  shared::mqtt.emplace(                   // in-place create
      MQTT::ConnOpts(net::host_id(),      //
                     mqtt_cfg["uri"],     //
                     mqtt_cfg["user"],    //
                     mqtt_cfg["passwd"]), //
      this                                // register this as message handler
  );

  if (!MQTT::hold_for_connection(60000)) esp_restart();
}

void Core::start_sntp() noexcept {
  Sntp::Opts opts;

  auto ntp_servers = Binder::ntp();

  opts.servers[0] = ntp_servers[0];
  opts.servers[1] = ntp_servers[1];
  opts.notify_task = xTaskGetCurrentTaskHandle();

  Sntp sntp(opts);

  uint32_t notify_val = 0;

  xTaskNotifyWait(0, ULONG_MAX, &notify_val, pdMS_TO_TICKS(10000));

  if (notify_val != Sntp::READY) {
    ESP_LOGW(TAG, "SNTP exceeded 10s");
    esp_restart();
  }
}

void Core::track_heap() noexcept {
  auto msg = message::Run();

  if (msg.isHeapLow()) {
    esp_restart();
  }

  MQTT::send(std::move(msg));
}

void Core::wantMessage(message::InWrapped &msg) {
  const char *kind = msg->kindFromFilter();

  if (kind[0] == 'p' && kind[1] == 'r') {
    msg->want(DocKinds::PROFILE);
    return;
  }

  if (kind[0] == 'r' && kind[1] == 'e') {
    msg->want(DocKinds::RESTART);
    return;
  }

  if (kind[0] == 'o' && kind[1] == 't') {
    msg->want(DocKinds::OTA);
    return;
  }

  if (kind[0] == 'b' && kind[1] == 'i') {
    msg->want(DocKinds::BINDER);
    return;
  }
}

} // namespace ruth
