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
#include "lightdesk/lightdesk.hpp"
#include "readings/text.hpp"

namespace ruth {

using TR = reading::Text;

bool LightDeskControl::config() {
  auto rc = true;
  const float bass_mag_floor = _desk->bassMagnitudeFloor();
  const float major_peak_roc_floor = _desk->majorPeakMagFloor();

  printf("\nbass_mag_floor(%10.1f) major_peak_roc_floor(%10.1f)\n\n",
         bass_mag_floor, major_peak_roc_floor);

  return rc;
}

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

bool LightDeskControl::objectSizes() {
  auto rc = true;

  printf("lightdesk(%5u) fx(%5u) dmx(%u) i2s(%5u) pinspot(%5u) control(%5u)\n",
         sizeof(LightDesk_t), sizeof(FxType_t), sizeof(Dmx_t), sizeof(I2s_t),
         sizeof(PinSpot_t), sizeof(LightDeskControl_t));

  printf("\n");

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
  printf("%-*smode(%3s) ac_power(%3s)\n", indent_size, "lightdesk:", stats.mode,
         stats.ac_power ? "on" : "off");
  printf("%sframe prepare min(%4uµs) curr(%4uµs) max(%4uµs)\n", indent,
         stats.frame_prepare.min, stats.frame_prepare.curr,
         stats.frame_prepare.max);

  printf("%sel wire dance_floor(%4u) entry(%4u)\n", indent,
         stats.elwire.dance_floor, stats.elwire.entry);

  printf("%sfx: active(%s) next(%s) prev(%s)\n", indent, fxDesc(fx.active),
         fxDesc(fx.next), fxDesc(fx.prev));

  // dmx
  const float frame_ms = (float)dmx.frame.us / 1000.f;
  printf("\n");
  printf("%-*s%02.02ffps frame(%6.3fms) expected_fps(%02.02f) shorts(%llu) "
         "white_space(%7.3fms)\n",
         indent_size, "dmx:", dmx.fps, frame_ms, dmx.frame.fps_expected,
         dmx.frame.shorts, (float)dmx.frame.white_space_us / 1000.0);

  printf("%sframe_update: curr(%4lluµs) min(%4lluµs) max(%4lluµs)\n", indent,
         dmx.frame.update.curr, dmx.frame.update.min, dmx.frame.update.max);
  printf("%stx: curr(%02.02fms) min(%02.02fms) max(%02.02fms)\n", indent,
         dmx.tx.curr, dmx.tx.min, dmx.tx.max);
  printf("%sbusy_wait(%lld)\n", indent, dmx.busy_wait);

  // i2s
  printf("\n");
  const float raw_kbps = i2s.rate.raw_bps / 1024.0;
  const uint32_t samples_ps = i2s.rate.samples_per_sec;
  const float pps = i2s.rate.fft_per_sec;
  printf("%-*s%.02f Kb/s %11u samples/s %11.2f fft/s\n", indent_size,
         "i2s:", raw_kbps, samples_ps, pps);

  const float i2s_rx_ms = (float)i2s.durations.rx_avg_us / 1000.0;
  printf("%sdurations: fft(%7.1fµs) rx(%0.2fms)\n", indent,
         i2s.durations.fft_avg_us, i2s_rx_ms);

  printf("%sraw min(%8d) max(%8d)\n", indent, i2s.raw_val.min24,
         i2s.raw_val.max24);

  printf("%sfft: bin_width(%7.2fHz) magnitude(%10.2f,%10.2f) \n", indent,
         i2s.config.freq_bin_width, i2s.magnitude.min, i2s.magnitude.max);

  printf("%smag_floor: generic(%8.2f) bass(%8.2f)\n", indent, i2s.mag_floor,
         i2s.bass_mag_floor);

  printf("%smpeak: freq(%8.2f) mag(%8.2f)\n", indent, i2s.mpeak.freq,
         i2s.mpeak.mag);

  printf("\n");

  return rc;
}

} // namespace ruth
