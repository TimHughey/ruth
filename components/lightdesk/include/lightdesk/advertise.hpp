/*
    lightdesk/lightdesk.hpp - Ruth Light Desk
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

#include "io/io.hpp"
#include "ru_base/time.hpp"
#include "ru_base/types.hpp"
#include "state/state.hpp"

#include <chrono>
#include <memory>

namespace ruth {

namespace desk {
class Advertise;
typedef std::shared_ptr<Advertise> shAdvertise;

class Advertise : public std::enable_shared_from_this<Advertise> {
public:
  static constexpr csv TAG = "advertise";

public:
  // Advertise(const Port service_port, const Port msg_port)
  //     : service_port(service_port), msg_port(msg_port) {}

  Advertise(const Port service_port) : service_port(service_port) {}

public: // static function to create, access and reset shared LightDesk
        // static shAdvertise create(const Port service_port, const Port msg_port);
  // static shAdvertise create(const Port service_port, const Port msg_port);
  static shAdvertise create(const Port service_port);
  static shAdvertise ptr();
  static void reset();

  // general public API
  shAdvertise init();
  static void stop();

private:
  // shAdvertise addService();
  void run();
  static inline void start([[maybe_unused]] void *data) { ptr()->run(); }

private:
  // order dependent
  const Port service_port;
  // const Port msg_port;

  // order independent
  std::unique_ptr<char> name;
  // std::unique_ptr<char> msg_port_txt;
  static constexpr csv service = "_ruth";
  static constexpr csv protocol = "_tcp";
};

} // namespace desk
} // namespace ruth
