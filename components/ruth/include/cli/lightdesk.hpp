/*
    cli/lightdesk.hpp - Ruth Command Line Interface for the LightDesk
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

#ifndef _ruth_cli_lightdesk_hpp
#define _ruth_cli_lightdesk_hpp

#include <esp_console.h>

#include "lightdesk/lightdesk.hpp"

namespace ruth {

typedef class LightDeskCli LightDeskCli_t;

using namespace lightdesk;

class LightDeskCli {

public:
  LightDeskCli(){};

  void init() { registerArgTable(); }

private:
  typedef enum { STROBE } Args_t;

private:
  static uint32_t convertHex(const char *str);
  static int execute(int argc, char **argv);

  static int pauseDesk(LightDesk_t *lightdesk);

  static int resumeDesk(LightDesk_t *lightdesk);
  static void reportStats(LightDesk_t *lightdesk);
  void registerArgTable();

  static bool validate(Args_t arg);
};

} // namespace ruth

#endif
