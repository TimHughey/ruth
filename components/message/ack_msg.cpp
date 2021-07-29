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

#include "message/ack_msg.hpp"

namespace message {

IRAM_ATTR Ack::Ack(const char *refid) : message::Out(128) {
  _filter.addLevel("mut");
  _filter.addLevel("cmdack");
  _filter.addLevel(refid);
}

IRAM_ATTR void Ack::assembleData(JsonObject &root) {}

} // namespace message
