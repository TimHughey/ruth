
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

#include "ArduinoJson.h"
#include "ru_base/types.hpp"

#include <array>
#include <concepts>
#include <esp_err.h>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

namespace ruth {

class Binder {

public:
  Binder() noexcept;

  inline JsonObjectConst doc_at_key(auto &&key) noexcept {
    JsonVariantConst v = root[key];
    return v.as<JsonObjectConst>();
  }

  inline const char *env() noexcept { return meta["env"] | "test"; }

  const string &host_id() const noexcept { return _host_id; }
  const string &hostname() noexcept;

  const string mac_address() const noexcept { return mac_address(6, csv("")); }
  const string mac_address(std::size_t want_bytes, csv &&sep) const noexcept;

  inline const auto ntp() noexcept { return root["ntp"].as<JsonArrayConst>(); }

private:
  static void check_error(esp_err_t err, const char *desc) noexcept;
  void parse();

public:
  // order dependent
  DynamicJsonDocument doc;

private:
  std::array<uint8_t, 6> _mac_address;
  string _host_id;
  string _hostname;

private:
  // order independent
  JsonObjectConst root;
  JsonObjectConst meta;

public:
  static constexpr auto TAG{"Binder"};
};

} // namespace ruth
