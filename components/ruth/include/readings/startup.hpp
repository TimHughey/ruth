/*
    celsius.hpp - Ruth Celsius Reading
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

#ifndef _ruth_reading_startup_hpp
#define _ruth_reading_startup_hpp

#include <esp_ota_ops.h>
#include <esp_system.h>
#include <sys/time.h>
#include <time.h>

#include "readings/remote.hpp"

namespace ruth {
namespace reading {
typedef class Startup Startup_t;

class Startup : public Remote_t {
private:
  const esp_app_desc_t *app_desc_;
  const char *reset_reason_;

public:
  Startup();
  static const char *decodeResetReason(esp_reset_reason_t reason);

protected:
  virtual void populateMessage(JsonDocument &doc);
};
} // namespace reading
} // namespace ruth

#endif // startup_reading_hpp
