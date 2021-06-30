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
#include "celsius_msg.hpp"

namespace ds {

Celsius::Celsius(const Opts &opts) : message::Out(512) {
  _filter.addLevel("ds");
  _filter.addLevel("celsius");
  _filter.addLevel(opts.ident);

  JsonObject root = rootObject();
  root["mut"] = false;

  switch (opts.status) {

  case OK:
    _filter.addLevel("ok");
    root["val"] = opts.val;
    root["read_us"] = opts.read_us;
    break;

  case ERROR:
    _filter.addLevel("error");
    root["code"] = opts.error;
    break;
  }
}

void Celsius::assembleData(JsonObject &root) {}

} // namespace ds
