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

#include "states_msg.hpp"

namespace ds {

IRAM_ATTR States::States(const char *ident) : message::Out(1024), _start_at(now()) {
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

IRAM_ATTR uint64_t States::now() {
  struct timeval time_now;

  uint64_t us_since_epoch;

  gettimeofday(&time_now, nullptr);

  us_since_epoch = 0;
  us_since_epoch += time_now.tv_sec * 1000000L; // convert seconds to microseconds
  us_since_epoch += time_now.tv_usec;           // add microseconds since last second

  return us_since_epoch;
}

} // namespace ds
