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
#include "status_msg.hpp"

namespace pwm {

Status::Status(const char *ident) : message::Out(512) {
  _filter.addLevel("mut");
  _filter.addLevel("status");
  _filter.addLevel(ident);
}

void Status::addPin(uint8_t pin_num, const char *status) {
  JsonObject obj = rootObject();
  JsonArray pins = obj["pins"];
  if (!pins) {
    pins = obj.createNestedArray("pins");
  }

  auto pin_status = pins.createNestedArray();
  pin_status.add(pin_num);
  pin_status.add(status);
}

void Status::assembleData(JsonObject &root) {}

} // namespace pwm
