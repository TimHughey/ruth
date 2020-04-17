/*
    types.hpp - Master Control Command Base Class
    Copyright (C) 2017  Tim Hughey

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

#ifndef ruth_cmd_types_h
#define ruth_cmd_types_h

#include <bitset>
#include <cstdlib>
#include <map>
#include <string>

#include "devs/base.hpp"

namespace ruth {

typedef enum class CmdType {
  unknown,
  none,
  timesync,
  setswitch,
  heartbeat,
  setname,
  restart,
  enginesSuspend,
  otaHTTPS,
  pwm
} CmdType_t;

typedef class CmdTypeMap CmdTypeMap_t;
class CmdTypeMap {
public:
  static CmdTypeMap_t *instance();
  static CmdType_t fromByte(char byte) {
    return instance()->decodeByte(byte);
  }
  static CmdType_t fromString(const string_t &cmd) {
    return instance()->find(cmd);
  }

private:
  CmdTypeMap();

  CmdType_t decodeByte(char byte);
  CmdType_t find(const string_t &cmd);
};

} // namespace ruth
#endif