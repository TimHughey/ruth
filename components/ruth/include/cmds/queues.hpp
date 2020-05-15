/*
    queues.hpp - Ruth Command Queues Class
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

#ifndef ruth_cmd_queues_h
#define ruth_cmd_queues_h

#include <cstdlib>
#include <memory>
#include <string>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <sys/time.h>
#include <time.h>

#include "misc/local_types.hpp"

using std::unique_ptr;
using std::vector;

namespace ruth {

typedef class CmdQueues CmdQueues_t;
class CmdQueues {
private:
  vector<cmdQueue_t> _queues;

  CmdQueues(){}; // SINGLETON!

public:
  void add(cmdQueue_t &cmd_q) { _queues.push_back(cmd_q); };
  static vector<cmdQueue_t> &all() { return _instance_()->queues(); };
  static CmdQueues_t *_instance_();
  vector<cmdQueue_t> &queues() { return _queues; };
  static void registerQ(cmdQueue_t &cmd_q);

  const unique_ptr<char[]> debug();
};
} // namespace ruth

#endif
