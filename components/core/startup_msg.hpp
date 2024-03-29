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

#ifndef ruth_core_startup_msg_hpp
#define ruth_core_startup_msg_hpp

#include "message/out.hpp"

namespace message {

class Startup : public Out {
public:
  Startup();
  ~Startup() = default;

private:
  void assembleData(JsonObject &data);
  const char *resetReason();
};
} // namespace message
#endif
