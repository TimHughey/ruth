/*
  Ruth
  (C)opyright 2021  Tim Hughey

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

#include <esp_attr.h>
#include <esp_timer.h>

#include "message/states_msg.hpp"

namespace message {

IRAM_ATTR States::States(const char *ident) : message::Out(1024), _start_at(esp_timer_get_time()) {
  _filter.addLevel("mut");
  _filter.addLevel("status");
  _filter.addLevel(ident);
}

IRAM_ATTR void States::addPin(uint8_t pin_num, const char *status) {
  JsonObject obj = rootObject();
  JsonArray pins = obj["pins"];
  if (!pins) {
    pins = obj.createNestedArray("pins");
  }

  auto pin_status = pins.createNestedArray();
  pin_status.add(pin_num);
  pin_status.add(status);
}

IRAM_ATTR void States::assembleData(JsonObject &root) {
  auto metrics = root.createNestedObject("metrics");
  metrics["read"] = _read_us;

  if (_status == OK) {
    _filter.addLevel("ok");
  } else {
    _filter.addLevel("error");
  }
}

IRAM_ATTR void States::finalize() { _read_us = esp_timer_get_time() - _start_at; }

} // namespace message
