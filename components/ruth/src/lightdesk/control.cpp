/*
    lightdesk/control.cpp - Ruth Light Desk Control
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

#include "external/ArduinoJson.h"

#include "lightdesk/control.hpp"
#include "lightdesk/fx_defs.hpp"
#include "lightdesk/lightdesk.hpp"
#include "readings/text.hpp"

namespace ruth {

using TR = reading::Text;

static LightDeskControl_t *__singleton__ = nullptr;

LightDeskControl::LightDeskControl() {}

bool LightDeskControl::command(MsgPayload_t &msg) {
  bool rc = false;
  StaticJsonDocument<256> doc;

  auto err = deserializeMsgPack(doc, msg.data());

  auto root = doc.as<JsonObject>();

  if (err) {
    TR::rlog("LightDesk command parse failure: %s", err.c_str());
    return rc;
  }

  auto dance_obj = root["dance"].as<JsonObject>();
  auto mode_obj = root["mode"].as<JsonObject>();

  if (dance_obj) {
    _dance_interval = dance_obj["interval_secs"];

    rc = dance(_dance_interval);

    TR::rlog("LightDesk dance with interval=%3.2f %s", _dance_interval,
             (rc) ? "started" : "failed");
  }

  if (mode_obj) {
    const bool ready_flag = mode_obj["ready"];
    const bool stop_flag = mode_obj["stop"];

    if (ready_flag) {
      ready();
    }
    if (stop_flag) {
      stop();
    }

    rc = true;
  }

  return rc;
}

bool LightDeskControl::isRunning() {
  auto rc = false;

  if ((_mode == DANCE) || (_mode == READY)) {
    rc = true;
  }

  return rc;
}

LightDeskControl_t *LightDeskControl::i() {
  if (__singleton__ == nullptr) {
    __singleton__ = new LightDeskControl();
  }

  return __singleton__;
}

bool LightDeskControl::setMode(LightDeskMode_t mode) {
  auto rc = true;
  _mode = mode;

  switch (mode) {
  case INIT:
    rc = false;
    break;

  case STOP:
    if (_desk != nullptr) {
      LightDesk *desk = _desk;
      _desk = nullptr;

      rc = desk->request(_request);
      delete desk;
    }
    break;

  case COLOR:
  case DANCE:
  case FADE_TO:
  case READY:
    if (_desk == nullptr) {
      _desk = new LightDesk();
    }

    rc = _desk->request(_request);

    break;

  default:
    rc = false;
    break;
  }

  return rc;
}

bool LightDeskControl::stats() {
  auto rc = true;
  static const char *indent = "                ";
  const LightDeskStats_t &stats = _desk->stats();
  const DmxStats_t &dmx = stats.dmx;
  const FxStats_t &fx = stats.fx;

  printf("\n");
  printf("lightdesk:      mode=%s object_size=%u\n", stats.mode,
         stats.object_size);

  printf("\n");
  printf("lightdesk_fx:   basic=%llu active=%s next=%s prev=%s\n", fx.fx.basic,
         fxDesc(fx.fx.active), fxDesc(fx.fx.next), fxDesc(fx.fx.prev));

  printf("%sinterval curr=%4.2fs min=%4.2fs max=%4.2fs base=%4.2fs\n", indent,
         fx.interval.current, fx.interval.min, fx.interval.max,
         fx.interval.base);
  printf("%sobject_size=%u\n", indent, fx.object_size);

  printf("\n");

  const float frame_ms = (float)dmx.frame.us / 1000.f;

  printf("dmx:            %02.02ffps frame=%5.3fms shorts=%llu ", dmx.fps,
         frame_ms, dmx.frame.shorts);
  printf("frame_update: curr=%lluµs min=%lluµs max=%lluµs\n",
         dmx.frame.update.curr, dmx.frame.update.min, dmx.frame.update.max);
  printf("%stx curr=%02.02fms min=%02.02fms max=%02.02fms\n", indent,
         dmx.tx.curr, dmx.tx.min, dmx.tx.max);
  printf("%sbusy_wait=%lld object_size=%u\n", indent, dmx.busy_wait,
         dmx.object_size);

  const uint32_t last_pinspot = static_cast<uint32_t>(PINSPOT_FILL);

  for (uint32_t i = 0; i <= last_pinspot; i++) {
    printf("\n");
    const PinSpotStats_t &pinspot = stats.pinspot[i];
    TextBuffer<16> name;
    name.printf("pinspot %02d:", i);

    printf("%-16snotify count=%llu retries=%llu failures=%llu\n", name.c_str(),
           pinspot.notify.count, pinspot.notify.retries,
           pinspot.notify.failures);
    printf("%sobject_size=%u\n", indent, pinspot.object_size);
  }

  printf("\n");

  return rc;
}

bool IRAM_ATTR LightDesk::taskNotify(NotifyVal_t val) const {
  const uint32_t v = static_cast<uint32_t>(val);
  TickType_t wait_ticks = pdMS_TO_TICKS(1);
  const uint32_t max_wait_ticks = pdMS_TO_TICKS(20);

  UBaseType_t notified = pdFAIL;

  do {
    notified = xTaskNotify(task(), v, eSetValueWithoutOverwrite);

    if (notified == pdFAIL) {
      vTaskDelay(wait_ticks);
      wait_ticks++; // backoff one tick at a time if busy
    }

  } while ((notified == pdFAIL) && (wait_ticks <= max_wait_ticks));

  return (notified == pdPASS) ? true : false;
}

} // namespace ruth
