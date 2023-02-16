//  Ruth
//  Copyright (C) 2020  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "headunit/headunit.hpp"

namespace ruth {

class AcPower : public HeadUnit {
public:
  AcPower(csv id) noexcept;
  ~AcPower() noexcept override;

public:
  void dark() noexcept override { setLevel(false); }
  inline void handle_msg(JsonDocument &doc) noexcept override { setLevel(doc[moduleId()] | false); }

  bool off() noexcept { return setLevel(false); }
  bool on() noexcept { return setLevel(true); }
  bool status() noexcept; // in .cpp to limit gpio impl

private:
  bool setLevel(bool level) noexcept; // in .cpp to limit gpio impl
};

} // namespace ruth
