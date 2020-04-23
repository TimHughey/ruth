/*
    cmd_pwm.hpp - Ruth Command PWM Class
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

#ifndef ruth_cmd_pwm_hpp
#define ruth_cmd_pwm_hpp

#include <cstdlib>
#include <memory>
#include <string>

#include <freertos/FreeRTOS.h>
#include <sys/time.h>
#include <time.h>

#include "cmds/base.hpp"
#include "misc/elapsedMillis.hpp"
#include "misc/local_types.hpp"

using std::unique_ptr;

namespace ruth {

typedef class cmdPWM cmdPWM_t;
class cmdPWM : public Cmd {
private:
  uint32_t _duty; // absolute duty *or* starting duty when fading
  // fading parameters
  uint32_t _direction = 2;      // 0 = rev, 1 = fwd, 2 = don't fade
  uint32_t _step_num = 0;       // duty steps from initial duty
  uint32_t _duty_cycle_num = 0; // tick cycles per step
  uint32_t _duty_scale = 0;     // unclear what this parameter does

public:
  cmdPWM(JsonDocument &doc, elapsedMicros &parse);
  cmdPWM(const cmdPWM_t *cmd)
      : Cmd{cmd}, _duty(cmd->_duty), _direction(cmd->_direction),
        _step_num(cmd->_step_num), _duty_cycle_num(cmd->_duty_cycle_num),
        _duty_scale(cmd->_duty_scale){};

  uint32_t direction() { return _direction; };
  uint32_t duty() { return _duty; };
  uint32_t duty_cycle_num() { return _duty_cycle_num; };
  uint32_t duty_scale() { return _duty_scale; };
  uint32_t step_num() { return _step_num; };

  bool is_fade() { return (_direction < 2) ? true : false; };

  bool IRAM_ATTR process();

  size_t size() const { return sizeof(cmdPWM_t); };

  const unique_ptr<char[]> debug();
};
} // namespace ruth

#endif
