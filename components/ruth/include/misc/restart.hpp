/*
    restart.hpp -- Abstraction for esp_restart()
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

#ifndef ruth_restart_hpp
#define ruth_restart_hpp

#include <sys/time.h>
#include <time.h>

#include <esp_system.h>

#include "readings/readings.hpp"

namespace ruth {

typedef class Restart Restart_t;

class Restart {

public:
  Restart();

  ~Restart();

  static void now() { _instance_()->_now_(); };
  static void restart(const char *text = nullptr, const char *func = nullptr,
                      uint32_t reboot_delay_ms = 0);

private:
  static Restart_t *_instance_();
  void _now_();
};

} // namespace ruth

#endif // restart.hpp
