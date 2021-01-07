/*
    cli/random.cpp - Ruth CLI for random number functions
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

#include "cli/random.hpp"
#include "misc/random.hpp"

namespace ruth {

int RandomCli::execute(int argc, char **argv) {
  printDiceRollStats();
  return 0;
}

void RandomCli::registerArgTable() {
  static esp_console_cmd_t cmd = {};
  cmd.command = "dicestats";
  cmd.help = "Display the current die roll stats";
  cmd.hint = NULL;
  cmd.func = &execute;

  esp_console_cmd_register(&cmd);
}

}; // namespace ruth
