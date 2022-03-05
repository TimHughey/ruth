/*
    binder.hpp -- Abstraction for ESP32 SPIFFS
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

#ifndef _ruth_binder_hpp
#define _ruth_binder_hpp

#include <memory>

#include "ArduinoJson.h"

typedef class Binder Binder_t;

class Binder {

public:
  Binder(){}; // SINGLETON
  static void init() { i()->parse(); }
  static Binder_t *instance() { return i(); }

  static const char *env() { return i()->meta["env"] | "test"; }
  static const JsonObject mqtt() {
    JsonObject mqtt = i()->root["mqtt"];

    return std::move(mqtt);
  }
  static const JsonArray ntp() {
    JsonArray ntp = i()->root["ntp"].as<JsonArray>();

    return std::move(ntp);
  }
  static int64_t now();
  static const JsonObject wifi() {
    JsonObject wifi = i()->root["wifi"];

    return std::move(wifi);
  }

private:
  static Binder *i();

  void parse();

private:
  JsonObject root;
  JsonObject meta;
};

#endif
