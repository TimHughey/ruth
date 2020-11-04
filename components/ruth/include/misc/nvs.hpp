/*
    nvs.hpp -- Abstraction for ESP32 NVS
    Copyright (C) 2019  Tim Hughey

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

#ifndef _ruth_nvs_hpp
#define _ruth_nvs_hpp

#include <string>

#include <sys/time.h>
#include <time.h>

#include <esp_system.h>
#include <nvs.h>
#include <nvs_flash.h>

namespace ruth {

typedef class NVS NVS_t;

class NVS {
private:
  NVS();
  ~NVS();
  static NVS *_instance_();

public:
  static NVS_t *init();
};
} // namespace ruth

#endif // nvs.hpp
