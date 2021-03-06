/*
     engines/event_bits.hpp - Ruth Engine Event Bits
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

#ifndef _ruth_engine_event_bits_hpp
#define _ruth_engine_event_bits_hpp

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

namespace ruth {
typedef struct {
  EventBits_t need_bus;
  EventBits_t engine_running;
  EventBits_t devices_available;
  EventBits_t temp_available;
  EventBits_t temp_sensors_available;
} engineEventBits_t;

}; // namespace ruth

#endif
