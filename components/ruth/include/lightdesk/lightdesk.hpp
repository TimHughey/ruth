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

#ifndef _ruth_lightdesk_hpp
#define _ruth_lightdesk_hpp

#include <memory>

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

// #include "lightdesk/headunits/elwire.hpp"
// #include "lightdesk/headunits/ledforest.hpp"
#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "misc/random.hpp"
#include "protocols/dmx.hpp"

namespace ruth {
namespace lightdesk {

typedef class LightDesk LightDesk_t;

class LightDesk {

public:
  LightDesk();
  ~LightDesk();

  void start();
  void stop();

private: // headunits
         // inline DiscoBall_t *discoball() { return _discoball; }
         // inline ElWire_t *elWireDanceFloor() { return
         // _elwire[ELWIRE_DANCE_FLOOR]; } inline ElWire_t *elWireEntry() {
         // return _elwire[ELWIRE_ENTRY]; }
private:
  void init();

private:
  esp_err_t _init_rc = ESP_FAIL;

  std::shared_ptr<Dmx> _dmx;

  // AcPower_t *_ac_power = nullptr;
  // ElWire_t *_elwire[2] = {};
  // LedForest_t *_led_forest = nullptr;
  // DiscoBall_t *_discoball = nullptr;
};

} // namespace lightdesk
} // namespace ruth

#endif
