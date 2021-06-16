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

#ifndef core_sntp_hpp
#define core_sntp_hpp

#include <memory>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

namespace ruth {

class Sntp {
public:
  struct Opts {
    TaskHandle_t notify_task = nullptr;
    uint32_t notify_val = 0;
    const char *servers[2] = {};
    uint32_t check_ms = 250;
  };

public:
  Sntp(const Opts &opts);
  ~Sntp();

  // void ensureTimeIsSet();

private:
  static void checkStatus(TimerHandle_t handle);

private:
  Opts _opts;

  TimerHandle_t _timer = nullptr;
};

}; // namespace ruth

#endif
