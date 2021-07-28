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
#include "relhum_msg.hpp"

namespace i2c {

RelHum::RelHum(const Opts &opts) : message::Out(512) {
  _filter.addLevel("immut");
  _filter.addLevel("relhum");
  _filter.addLevel(opts.ident);

  JsonObject root = rootObject();

  switch (opts.status) {

  case OK: {
    _filter.addLevel("ok");
    root["temp_c"] = opts.temp_c;
    root["relhum"] = opts.relhum;

    JsonObject metrics = root.createNestedObject("metrics");
    metrics["read"] = opts.read_us;
  } break;

  case ERROR:
    _filter.addLevel("error");
    root["code"] = opts.error;
    break;

  case CRC_MISMATCH:
    _filter.addLevel("crc_mismatch");
    break;
  }
}

void RelHum::assembleData(JsonObject &root) {}

} // namespace i2c
