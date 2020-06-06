/*
    include/pwm/sequence/basic.hpp - Ruth PWM Basic Sequence Class
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

#ifndef _ruth_pwm_sequence_basic_hpp
#define _ruth_pwm_sequence_basic_hpp

#include <memory>
#include <string>
#include <vector>

#include "external/ArduinoJson.h"
#include "local/types.hpp"

#include "devs/pwm/sequence/sequence.hpp"

namespace ruth {
namespace pwm {

using std::vector;

typedef class Basic Basic_t;

class Basic : public Sequence {
public:
  Basic(const char *pin, ledc_channel_config_t *chan, JsonObject &obj);
  ~Basic();

protected:
  static void loop(void *task_data) {
    Basic_t *obj = (Basic_t *)task_data;

    obj->_loop();
  }

private:
  void _loop();

private:
  vector<Step_t *> _steps = {}; // the actual steps
  bool _repeat = false;         // should sequence repeat or execute once?
};

} // namespace pwm
} // namespace ruth

#endif
