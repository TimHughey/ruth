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

bool LightDeskControl::handleCommand(MsgPayload_t &msg) {
  bool rc = false;
  StaticJsonDocument<256> doc;

  auto err = deserializeMsgPack(doc, msg.data());

  const auto root = doc.as<JsonObject>();

  if (err) {
    TR::rlog("LightDesk command parse failure: %s", err.c_str());
    return rc;
  }

  const auto mode_obj = root["mode"].as<JsonObject>();

  if (mode_obj) {
    const bool ready_flag = mode_obj["ready"];
    const bool stop_flag = mode_obj["stop"];
    const bool dance_flag = mode_obj["dance"];

    if (ready_flag) {
      rc = ready();

    } else if (dance_flag) {
      const auto dance_interval = mode_obj["opts"]["interval_secs"] | 23.3;

      rc = dance(dance_interval);

    } else if (stop_flag) {
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

bool LightDeskControl::setMode(LightDeskMode_t mode) {
  _mode = mode;

  if (_desk == nullptr) {
    _desk = new LightDesk();
  }

  auto rc = _desk->request(_request);

  return rc;
}

bool LightDeskControl::stats() {
  auto rc = true;
  static const char *indent = "                    ";
  const size_t indent_size = strlen(indent);
  const LightDeskStats_t &stats = _desk->stats();
  const DmxStats_t &dmx = stats.dmx;
  const FxStats_t &fx = stats.fx;

  printf("\n");
  printf("%-*smode=%s object_size=%u\n", indent_size, "lightdesk:", stats.mode,
         stats.object_size);

  printf("\n");
  printf("%-*sbasic=%llu active=%s next=%s prev=%s\n", indent_size,
         "lightdesk_fx:", fx.fx.basic, fxDesc(fx.fx.active), fxDesc(fx.fx.next),
         fxDesc(fx.fx.prev));

  printf("%sinterval curr=%4.2fs min=%4.2fs max=%4.2fs base=%4.2fs\n", indent,
         fx.interval.current, fx.interval.min, fx.interval.max,
         fx.interval.base);
  printf("%sobject_size=%u\n", indent, fx.object_size);

  printf("\n");

  const float frame_ms = (float)dmx.frame.us / 1000.f;

  printf("%-*s%02.02ffps frame=%5.3fms shorts=%llu ", indent_size,
         "dmx:", dmx.fps, frame_ms, dmx.frame.shorts);

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

    printf("%-*snotify count=%llu retries=%llu failures=%llu\n", indent_size,
           name.c_str(), pinspot.notify.count, pinspot.notify.retries,
           pinspot.notify.failures);
    printf("%sobject_size=%u\n", indent, pinspot.object_size);
  }

  printf("\n");

  printf("%-*sobject_size=%u\n", indent_size,
         "lightdeskcontrol:", sizeof(LightDeskControl_t));

  printf("\n");

  return rc;
}

} // namespace ruth
