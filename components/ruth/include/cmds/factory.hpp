/*
    factory.hpp - Master Control Command Factory Class
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

#ifndef ruth_cmd_factory_h
#define ruth_cmd_factory_h

#include <cstdlib>
#include <string>
#include <vector>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <sys/time.h>
#include <time.h>

#include "cmds/base.hpp"
#include "cmds/network.hpp"
#include "cmds/ota.hpp"
#include "cmds/pwm.hpp"
#include "cmds/switch.hpp"
#include "cmds/types.hpp"
#include "external/ArduinoJson.hpp"
#include "misc/mcr_types.hpp"

namespace ruth {

typedef class CmdFactory CmdFactory_t;
class CmdFactory {
private:
  Cmd_t *manufacture(JsonDocument &doc, elapsedMicros &parse_elapsed);

public:
  CmdFactory();

  Cmd_t *fromRaw(JsonDocument &doc, rawMsg_t *raw);
};

} // namespace ruth

#endif
