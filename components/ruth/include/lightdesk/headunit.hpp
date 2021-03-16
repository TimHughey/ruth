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

#ifndef _ruth_dmx_headunit_device_hpp
#define _ruth_dmx_headunit_device_hpp

#include <unordered_set>

#include "external/ArduinoJson.h"
#include "local/types.hpp"

namespace ruth {
namespace lightdesk {

class HeadUnit {
public:
  HeadUnit() = default;
  virtual ~HeadUnit() = default;

  virtual void handleMsg(const JsonObject &obj) = 0;
  virtual void dark() = 0;
};

typedef std::shared_ptr<HeadUnit> spHeadUnit;
typedef std::unordered_set<spHeadUnit> HeadUnitTracker;
} // namespace lightdesk
} // namespace ruth

#endif
