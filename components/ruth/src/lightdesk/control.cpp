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
  const I2sStats_t &i2s = stats.i2s;
  const FxStats_t &fx = stats.fx;

  // lightdesk
  printf("\n");
  printf("%-*smode=%s object_size=%u\n", indent_size, "lightdesk:", stats.mode,
         stats.object_size);
  printf("%sframe prepare min(%4uµs) curr(%4uµs) max(%4uµs)\n", indent,
         stats.frame_prepare.min, stats.frame_prepare.curr,
         stats.frame_prepare.max);

  printf("\n");
  printf("%-*sbasic=%llu active=%s next=%s prev=%s\n", indent_size,
         "lightdesk_fx:", fx.fx.basic, fxDesc(fx.fx.active), fxDesc(fx.fx.next),
         fxDesc(fx.fx.prev));

  printf("%sinterval curr=%4.2fs min=%4.2fs max=%4.2fs base=%4.2fs\n", indent,
         fx.interval.current, fx.interval.min, fx.interval.max,
         fx.interval.base);
  printf("%sobject_size=%u\n", indent, fx.object_size);

  // dmx
  const float frame_ms = (float)dmx.frame.us / 1000.f;
  printf("\n");
  printf("%-*s%02.02ffps frame(%6.3fms) shorts(%llu) white_space(%7.3fms)\n",
         indent_size, "dmx:", dmx.fps, frame_ms, dmx.frame.shorts,
         (float)dmx.frame.white_space_us / 1000.0);

  printf("%sframe_update: curr(%4lluµs) min(%4lluµs) max(%4lluµs)\n", indent,
         dmx.frame.update.curr, dmx.frame.update.min, dmx.frame.update.max);
  printf("%stx: curr(%02.02fms) min(%02.02fms) max(%02.02fms)\n", indent,
         dmx.tx.curr, dmx.tx.min, dmx.tx.max);
  printf("%sbusy_wait(%lld) object_size(%u)\n", indent, dmx.busy_wait,
         dmx.object_size);

  // i2s
  printf("\n");
  const float raw_kbps = i2s.rate.raw_bps / 1024.0;
  const float samples_kbps = i2s.rate.samples_per_sec / 1000;
  printf("%-*srate: %.02fKbps %9.2fksps\n", indent_size, "i2s:", raw_kbps,
         samples_kbps);

  const float fft_ms = (float)i2s.durations.fft_avg_us / 1000.0;
  const float i2s_rx_ms = (float)i2s.durations.rx_avg_us / 1000.0;
  printf("%sdurations: fft(%0.2fms) rx(%0.2fms)\n", indent, fft_ms, i2s_rx_ms);

  printf("%sraw min(%8d) max(%8d)\n", indent, i2s.raw_val.min24,
         i2s.raw_val.max24);

  printf("%sfft: bin_width(%6.2fHz) magnitude(%10.2f,%10.2f) \n", indent,
         i2s.config.freq_bin_width, i2s.magnitude.min, i2s.magnitude.max);

  printf("%sobject_size(%u)\n", indent, i2s.object_size);

  // pinspots

  const uint32_t last_pinspot = static_cast<uint32_t>(PINSPOT_FILL);

  for (uint32_t i = 0; i <= last_pinspot; i++) {
    printf("\n");
    const PinSpotStats_t &pinspot = stats.pinspot[i];
    TextBuffer<16> name;
    name.printf("pinspot %02d:", i);

    printf("%-*sobject_size=%u\n", indent_size, name.c_str(),
           pinspot.object_size);
  }

  printf("\n");

  printf("%-*sobject_size=%u\n", indent_size,
         "lightdeskcontrol:", sizeof(LightDeskControl_t));

  printf("\n");

  return rc;
}

} // namespace ruth
