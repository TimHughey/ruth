/*
  Ruth
  (C)opyright 2020  Tim Hughey

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
#include <string_view>

#include <esp_log.h>

#include "ArduinoJson.h"
#include "binder.hpp"
#include "boot_msg.hpp"
#include "core.hpp"
#include "dev_pwm/pwm.hpp"
#include "engines.hpp"
#include "filter/filter.hpp"
#include "filter/out.hpp"
#include "misc/datetime.hpp"
#include "misc/status_led.hpp"
#include "network.hpp"
#include "ota/ota.hpp"
#include "run_msg.hpp"
#include "ruth_mqtt/mqtt.hpp"
#include "sntp.hpp"
#include "startup_msg.hpp"

namespace ruth {

static const char *TAG = "Core";

static Core __singleton__;
static StaticJsonDocument<2048> _profile;
static StaticJsonDocument<1024> _ota_cmd;
static firmware::OTA *_ota = nullptr;

Core::Core() : message::Handler("host", _max_queue_depth) {
  _heap_first = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  _heap_avail = heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

void Core::boot() {
  Core &core = __singleton__;

  StatusLED::init();
  Binder::init();

  StatusLED::brighter();

  auto wifi = Binder::wifi();

  Net::Opts net_opts;
  net_opts.ssid = wifi["ssid"];
  net_opts.passwd = wifi["passwd"];
  net_opts.notify_task = xTaskGetCurrentTaskHandle();

  StatusLED::brighter();
  Net::start(net_opts);
  uint32_t notify;
  xTaskNotifyWait(0, ULONG_MAX, &notify, pdMS_TO_TICKS(15000));

  if ((notify & Net::READY) == false) {
    ESP_LOGW(TAG, "while waiting for net ready received %u instead of %u", notify, Net::READY);
    vTaskDelay(pdMS_TO_TICKS(3333));
    esp_restart();
  }

  StatusLED::brighter();

  core.sntp(); // only returns if SNTP succeeds

  filter::Opts opts;
  opts.first_level = Binder::env();
  opts.host_id = Net::hostID();
  opts.hostname = Net::hostname();

  filter::Filter::init(opts);
  core.startMqtt();

  StatusLED::brighter();

  bool timeout;
  auto wrapped_msg = core.waitForMessage(3333, &timeout);

  if (timeout) {
    ESP_LOGW(TAG, "timeout waiting for profile");
    vTaskDelay(pdMS_TO_TICKS(3333));
    esp_restart();
  }

  StatusLED::brighter();
  const char *hostname = wrapped_msg->hostnameFromFilter();
  StatusLED::brighter();

  wrapped_msg->unpack(_profile);
  StatusLED::brighter();

  Net::setName(hostname);

  core.startEngines();

  core.trackHeap();
  StatusLED::percent(75);
  core.bootComplete();

  StatusLED::off();

  // if (Profile::watchStacks()) {
  //   _watcher = new Watcher();
  //   _watcher->start();
  // }
}
void Core::bootComplete() {
  // send our boot stats
  const char *profile_name = _profile["meta"]["name"] | "unknown";
  message::Boot msg(_stack_size, profile_name);
  MQTT::send(msg);

  // lower our priority to not compete with actual work
  if (uxTaskPriorityGet(nullptr) > _priority) {
    vTaskPrioritySet(nullptr, _priority);
  }

  // start our scheduled reports
  uint32_t report_ms = _profile["host"]["report_ms"] | 7000;
  _report_timer = xTimerCreate("core_report", pdMS_TO_TICKS(report_ms), pdTRUE, nullptr, &reportTimer);
  vTimerSetTimerID(_report_timer, this);
  xTimerStart(_report_timer, pdMS_TO_TICKS(0));

  uint32_t valid_ms = _profile["ota"]["valid_ms"] | 60000;
  firmware::OTA::handlePendingIfNeeded(valid_ms);
  firmware::OTA::captureBaseUrl(_profile["ota"]["base_url"]);
}

bool Core::enginesStarted() { return __singleton__._engines_started; }

void Core::loop() {
  Core &core = __singleton__;

  auto msg = core.waitForMessage();

  if (msg) {
    switch (msg->kind()) {
    case DocKinds::RESTART:
      esp_restart();
      break;

    case DocKinds::OTA:
      core.ota(std::move(msg));
      break;

    case DocKinds::BINDER:
      ESP_LOGI(TAG, "binder messages");
      break;

    case DocKinds::PROFILE:
      break;
    }
  }
}

void Core::ota(message::InWrapped msg) {

  using namespace firmware;

  // OTA already in progress, do nothing
  if (_ota) return;

  if (msg->unpack(_ota_cmd)) {
    const JsonObject cmd_root = _ota_cmd.as<JsonObject>();
    TaskHandle_t notify_task = xTaskGetCurrentTaskHandle();
    const char *file = cmd_root["file"] | "latest.bin";

    _ota = new firmware::OTA(notify_task, file, Net::ca_start());
    _ota->start();
  }

  while (_ota) {
    OTA::Notifies val;

    auto constexpr timeout = pdMS_TO_TICKS(1000);
    auto rc = xTaskNotifyWait(0x00, ULONG_MAX, (uint32_t *)&val, timeout);

    // notify timeout == OTA in progress, just track heap
    if (rc == pdFAIL) {
      trackHeap();
      continue;
    }

    if (val == OTA::Notifies::FINISH) {
      esp_restart();
      vTaskDelay(portMAX_DELAY);
    } else if (val == OTA::Notifies::START) {
      trackHeap();
      continue;
    }

    // if we've fallen through to here then cancel or error, clean up
    delete _ota;
    _ota = nullptr;
  }
}

void Core::reportTimer(TimerHandle_t handle) {
  Core *core = (Core *)pvTimerGetTimerID(handle);

  core->trackHeap();
}

void Core::sntp() {
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

void Core::startEngines() {
  StatusLED::brighter();
  // if the engines are already started obviously don't start them again.

  // secondly, if this host hasn't yet been assigned a name (it's new) then
  // don't engines.  by not starting the engines we prevent sending device
  // readings which in turn will prevent device creation centrally.

  // in other words, we only want to create devices centrally once this host
  // has been assigned a name.
  if (_engines_started || Net::hostIdAndNameAreEqual()) return;

  const JsonObject profile = _profile.as<JsonObject>();
  profile["hostname"] = Net::hostname();
  profile["unique_id"] = Net::macAddress();
  core::Engines::startConfigured(profile);

  _engines_started = true;
}

void Core::startMqtt() {

  const auto mqtt_cfg = Binder::mqtt();

  StatusLED::brighter();

  MQTT::ConnOpts opts;
  opts.client_id = Net::hostID();
  opts.uri = mqtt_cfg["uri"];
  opts.user = mqtt_cfg["user"];
  opts.passwd = mqtt_cfg["passwd"];
  opts.notify_task = xTaskGetCurrentTaskHandle();

  MQTT::initAndStart(opts);

  uint32_t notify;
  xTaskNotifyWait(0, ULONG_MAX, &notify, pdMS_TO_TICKS(60000));

  if ((notify & MQTT::CONNECTED) == false) {
    ESP_LOGW(TAG, "while waiting for mqtt connection received %u instead of %u", notify, MQTT::CONNECTED);
    vTaskDelay(pdMS_TO_TICKS(10000));
    esp_restart();
  }

  filter::Subscribe sub_filter;
  MQTT::subscribe(sub_filter);

  xTaskNotifyWait(0, ULONG_MAX, &notify, pdMS_TO_TICKS(10000));

  if ((notify & MQTT::READY) == false) {
    ESP_LOGW(TAG, "while waiting for mqtt ready received %u instead of %u", notify, MQTT::READY);
    vTaskDelay(pdMS_TO_TICKS(10000));
    esp_restart();
  }

  MQTT::registerHandler(this);

  message::Startup msg;
  MQTT::send(msg);

  StatusLED::off();
}

void Core::trackHeap() {
  message::Run msg;

  if (msg.isHeapLow()) {
    esp_restart();
  }

  MQTT::send(msg);
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
