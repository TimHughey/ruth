
// Ruth
// Copyright (C) 2022  Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "desk_msg/out.hpp"
#include "ru_base/types.hpp"

#include <ArduinoJson.h>
#include <concepts>
#include <esp_log.h>
#include <type_traits>

namespace ruth {
namespace desk {

class MsgOutWithInfo : public MsgOut {

protected:
  virtual void serialize_hook(JsonDocument &doc) noexcept override { add_heap_info(doc); }

public:
  // outbound messages
  inline MsgOutWithInfo(auto &msg_type) noexcept : MsgOut(msg_type) {}

  // inbound messages
  virtual ~MsgOutWithInfo() noexcept {} // prevent implicit copy/move

  inline MsgOutWithInfo(MsgOutWithInfo &&m) = default;
  MsgOutWithInfo &operator=(MsgOutWithInfo &&) = default;

private:
  void add_app_info(JsonDocument &doc) noexcept;
  void add_heap_info(JsonDocument &doc) noexcept;

  csv reset_reason() noexcept;

public:
  static constexpr csv module_id{"desk.msg.out"};
};

} // namespace desk
} // namespace ruth