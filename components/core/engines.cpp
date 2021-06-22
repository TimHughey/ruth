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

#include <esp_log.h>

#include "engine_pwm/pwm.hpp"
#include "engines.hpp"

namespace core {

void Engines::startConfigured(const JsonObject &profile) {
  const char *unique_id = profile["unique_id"];
  const JsonObject &pwm = profile["pwm"];

  if (pwm) {
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
}

} // namespace core
