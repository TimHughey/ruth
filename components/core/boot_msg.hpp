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

#ifndef ruth_core_boot_msg_hpp
#define ruth_core_boot_msg_hpp

#include <memory>

#include "out.hpp"

namespace message {

class Boot : public Out {
public:
  Boot(const size_t stack_size, const uint32_t elapsed_ms, const char *profile_name);
  ~Boot() = default;

private:
  void assembleData(JsonObject &data);
  const char *resetReason();

private:
  const size_t _stack_size;
  const uint32_t _elapsed_ms;
};
} // namespace message
#endif
