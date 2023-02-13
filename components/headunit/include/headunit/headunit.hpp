/*
    devs/dmx/headunit.hpp - Ruth Dmx Head Unit Device
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

#pragma once

#include "ArduinoJson.h"
#include "ru_base/types.hpp"
#include <memory>
#include <vector>

namespace ruth {

class HeadUnit;

typedef std::unique_ptr<HeadUnit> shHeadUnit;
typedef std::vector<shHeadUnit> HeadUnits;

class HeadUnit : std::enable_shared_from_this<HeadUnit> {
public:
  HeadUnit(csv module_id) : module_id(module_id) {}
  virtual ~HeadUnit() = default;

  virtual void dark() = 0;
  virtual void handleMsg(JsonDocument &doc) = 0;

  csv moduleId() { return module_id; }

private:
  string_view module_id;
};

} // namespace ruth
