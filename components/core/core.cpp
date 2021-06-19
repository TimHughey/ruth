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
#include "datetime.hpp"
#include "filter/filter.hpp"
#include "filter/out.hpp"
#include "mqtt.hpp"
#include "network.hpp"
#include "run_msg.hpp"
#include "sntp.hpp"
#include "startup_msg.hpp"
#include "status_led.hpp"

using namespace std::string_view_literals;

namespace ruth {

static const char *TAG = "Core";

static Core_t __singleton__;
static StaticJsonDocument<2048> _profile;

Core::Core() : message::Handler(_max_queue_depth) {
  _heap_first = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  _heap_avail = heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

void Core::bootComplete() {
  // send our boot stats
  message::Boot msg(_stack_size, _core_elapsed);
  MQTT::send(msg);

  // lower our priority to not compete with actual work
  if (uxTaskPriorityGet(nullptr) > _priority) {
    vTaskPrioritySet(nullptr, _priority);
  }

  // start our scheduled reports
  _report_timer = xTimerCreate("core_report", pdMS_TO_TICKS(_heap_track_ms), pdTRUE, nullptr, &reportTimer);
  vTimerSetTimerID(_report_timer, this);
  xTimerStart(_report_timer, pdMS_TO_TICKS(0));

  StatusLED::off();
}

bool Core::enginesStarted() { return __singleton__._engines_started; }

void Core::loop() {
  Core &core = __singleton__;
  // using TR = reading::Text;

  bool timeout;
  auto msg = core.waitForMessage(UINT32_MAX, timeout);

  if (timeout == true) return;

  ESP_LOGI(TAG, "got message %p", msg.get());

  // if (payload->matchSubtopic("restart")) {
  // TODO
  // esp_restart();
  // Restart("restart requested", __PRETTY_FUNCTION__);
  // }
  // else if (payload->matchSubtopic("ota")) {
  //
  //   if (_ota == nullptr) {
  //     _ota = new OTA();
  //     _ota->start();
  //     _ota->handleCommand(*payload); // handleCommand() logs as required
  //   }
  // } else if (payload->matchSubtopic("ota_cancel")) {
  //   if (_ota) {
  //     delete _ota;
  //     _ota = nullptr;
  //   }
  // }
  // else {
  // TR::rlog("[CORE] unknown message subtopic=\"%s\"", payload->subtopic());
  // }
}

void Core::sntp() {
  Sntp::Opts opts;

  auto ntp_servers = Binder::ntp();

  opts.servers[0] = ntp_servers[0];
  opts.servers[1] = ntp_servers[1];
  opts.notify_task = xTaskGetCurrentTaskHandle();

  Sntp sntp(opts);

  uint32_t notify_val = 0;
  elapsedMillis sntp_elapsed;
  xTaskNotifyWait(0, ULONG_MAX, &notify_val, pdMS_TO_TICKS(10000));
  sntp_elapsed.freeze();

  if (notify_val != Sntp::READY) {
    ESP_LOGW(TAG, "SNTP exceeded 10s");
    esp_restart();
  }
}

void Core::start() {
  Core &core = __singleton__;

  // core._app_task = app_task;

  StatusLED::init();
  Binder::init();

  StatusLED::brighter();

  auto wifi = Binder::wifi();

  Net::Opts net_opts;
  net_opts.ssid = wifi["ssid"];
  net_opts.passwd = wifi["passwd"];
  net_opts.notify_task = xTaskGetCurrentTaskHandle();

  Net::start(net_opts);
  uint32_t notify;
  xTaskNotifyWait(0, ULONG_MAX, &notify, pdMS_TO_TICKS(60000));

  if ((notify & Net::READY) == false) {
    ESP_LOGW(TAG, "while waiting for net ready received %u instead of %u", notify, Net::READY);
    vTaskDelay(pdMS_TO_TICKS(10000));
    esp_restart();
  }

  core.sntp(); // only returns if SNTP succeeds

  filter::Opts opts;
  opts.first_level = Binder::env();
  opts.host_id = Net::hostID();
  opts.hostname = Net::hostname();

  filter::Filter::init(opts);
  core.startMqtt();

  StatusLED::brighter();

  bool timeout;
  auto wrapped_msg = core.waitForMessage(30000, timeout);

  if (timeout) {
    ESP_LOGW(TAG, "timeout waiting for profile");
    vTaskDelay(pdMS_TO_TICKS(10000));
    esp_restart();
  }

  const char *hostname = wrapped_msg->filter(4);

  wrapped_msg->unpack(_profile);

  Net::setName(hostname);
  core.startEngines();

  // if (Binder::lightDeskEnabled() || Profile::lightDeskEnabled()) {
  //   _lightdesk = lightdesk::LightDesk::make_shared();
  // }

  core.trackHeap();
  core.bootComplete();
  // OTA::partitionHandlePendingIfNeeded();

  // if (Binder::cliEnabled()) {
  //   _cli->start();
  // }

  // if (Profile::watchStacks()) {
  //   _watcher = new Watcher();
  //   _watcher->start();
  // }
}

void Core::startEngines() {
  // if the engines are already started obviously don't start them again.

  // secondly, if this host hasn't yet been assigned a name (it's new) then
  // don't engines.  by not starting the engines we prevent sending device
  // readings which in turn will prevent device creation centrally.

  // in other words, we only want to create devices centrally once this host
  // has been assigned a name.
  if (_engines_started || Net::hostIdAndNameAreEqual()) {
    return;
  }

  // NOTE:
  //  startIfEnabled() checks if the engine is enabled in the Profile
  //   1. if so, creates the singleton and starts the engine
  //   2. if not, allocates nothing and does not start the engine
  // DallasSemi::startIfEnabled();
  // I2c::startIfEnabled();
  // PulseWidth::startIfEnabled();

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

  MQTT::registerHandler(this, "host"sv);

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
