/*
    Ruth
    Copyright (C) 2021  Tim Hughey

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

#ifndef _ruth_cli_dmx_hpp
#define _ruth_cli_dmx_hpp

#include <esp_console.h>

#include "protocols/dmx.hpp"

namespace ruth {
namespace cli {

class Dmx {

public:
  Dmx(){};

  void init() { registerArgTable(); }

private:
  static int execute(int argc, char **argv);
  void registerArgTable();
};
} // namespace cli

} // namespace ruth

#endif
