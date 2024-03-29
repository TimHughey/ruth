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

#ifndef core_run_message_hpp
#define core_run_message_hpp

#include <memory>

#include "message/out.hpp"

namespace message {

class Run : public Out {
public:
  Run();
  ~Run() = default;

  bool isHeapLow() const { return _heap_low; }

private:
  void assembleData(JsonObject &data);

private:
  bool _heap_low = false;
};
} // namespace message
#endif
