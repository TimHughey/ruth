/*
    network.hpp - Ruth Command Network Class
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

#ifndef ruth_cmd_network_hpp
#define ruth_cmd_network_hpp

#include "cmds/base.hpp"

using std::unique_ptr;

namespace ruth {

typedef class CmdNetwork CmdNetwork_t;
class CmdNetwork : public Cmd {
private:
  string_t _name;

public:
  CmdNetwork(JsonDocument &doc, elapsedMicros &e);
  CmdNetwork(Cmd *cmd) : Cmd(cmd) { _name = this->_name; };
  ~CmdNetwork(){};

  bool process();
  size_t size() const { return sizeof(CmdNetwork_t); };
  const unique_ptr<char[]> debug();
};

} // namespace ruth

#endif
