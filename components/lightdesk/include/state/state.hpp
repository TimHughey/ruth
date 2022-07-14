/*
    protocols/dmx.hpp - Ruth DMX Protocol Engine
    Copyright (C) 2020  Tim Hughey

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

#pragma once

#include "base/types.hpp"

namespace ruth {
namespace desk {

constexpr csv IDLE = "idle";
constexpr csv RENDER = "render";
constexpr csv SHUTDOWN = "shutdown";
constexpr csv ZOMBIE = "zombie";

struct State {
  State() = default;

  State &operator=(csv next_state) {
    state = next_state;
    return *this;
  }

  friend bool operator==(const State &lhs, csv &rhs) { return lhs.state.front() == rhs.front(); }

  void idle() { state = desk::IDLE; }
  void zombie() { state = desk::ZOMBIE; }

private:
  string_view state = desk::IDLE;
};

} // namespace desk
} // namespace ruth