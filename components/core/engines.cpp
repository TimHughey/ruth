/*
    Ruth
    Copyright (C) 2017  Tim Hughey

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

#include "core/engines.hpp"
#include "engine_ds/ds.hpp"
#include "engine_i2c/i2c.hpp"
#include "engine_pwm/pwm.hpp"
#include "lightdesk/lightdesk.hpp"
#include "ru_base/time.hpp"

#include <esp_log.h>

namespace ruth {

void Engines::startConfigured(const JsonObject &profile) {
  const char *unique_id = profile["unique_id"];
  const JsonObject &pwm = profile["pwm"];
  const JsonObject &ds = profile["dalsemi"];
  const JsonObject &i2c = profile["i2c"];
  const JsonObject &lightdesk = profile["lightdesk"];

  if (pwm && !lightdesk) {
    using namespace pwm;
    Engine::Opts opts;

    opts.unique_id = unique_id;
    opts.command.stack = pwm["command"]["stack"];
    opts.command.priority = pwm["command"]["pri"];
    opts.report.stack = pwm["report"]["stack"];
    opts.report.priority = pwm["report"]["pri"];
    opts.report.send_ms = pwm["report"]["send_ms"];

    Engine::start(opts);
  }

  if (ds) {
    using namespace ds;
    Engine::Opts opts;

    opts.unique_id = unique_id;
    opts.command.stack = ds["command"]["stack"];
    opts.command.priority = ds["command"]["pri"];
    opts.report.stack = ds["report"]["stack"];
    opts.report.priority = ds["report"]["pri"];
    opts.report.send_ms = ds["report"]["send_ms"];
    opts.report.loops_per_discover = ds["report"]["loops_per_discover"];

    Engine::start(opts);
  }

  if (i2c) {
    using namespace i2c;
    Engine::Opts opts;

    opts.unique_id = unique_id;
    opts.command.stack = i2c["command"]["stack"];
    opts.command.priority = i2c["command"]["pri"];
    opts.report.stack = i2c["report"]["stack"];
    opts.report.priority = i2c["report"]["pri"];
    opts.report.send_ms = i2c["report"]["send_ms"];
    opts.report.loops_per_discover = i2c["report"]["loops_per_discover"];

    Engine::start(opts);
  }

  if (lightdesk) {
    const LightDesk::Opts opts;
    LightDesk::create(opts)->init();
  }
}

} // namespace ruth
