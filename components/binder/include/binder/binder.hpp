
//  Ruth
//  Copyright (C) 2020  Tim Hughey
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

#pragma once

#include <memory>

#include "ArduinoJson.h"

namespace ruth {

class Binder;
namespace shared {
extern Binder binder;
}

class Binder {

public:
  Binder() = default;
  static void init() { shared::binder.parse(); }
  static Binder *instance() { return &shared::binder; }

  static const char *env() { return shared::binder.meta["env"] | "test"; }
  static const JsonObject mqtt() { return shared::binder.root["mqtt"]; }
  static const JsonArray ntp() { return shared::binder.root["ntp"].as<JsonArray>(); }

  static int64_t now();
  static const JsonObject wifi() { return shared::binder.root["wifi"]; }

private:
  void parse();

private:
  JsonObject root;
  JsonObject meta;
};

} // namespace ruth
