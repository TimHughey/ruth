/*
    types.hpp -- Profile Types
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

#ifndef _ruth_profile_types_hpp
#define _ruth_profile_types_hpp

namespace ruth {

// defines the Engines available in the profile
typedef enum {
  ENGINE_DALSEMI = 0,
  ENGINE_I2C,
  ENGINE_PWM,
  ENGINE_END_OF_LIST
} EngineTypes_t;

// defines the Engine Tasks available in the profile
typedef enum {
  TASK_CORE = 0,
  TASK_REPORT,
  TASK_COMMAND,
  TASK_END_OF_LIST
} EngineTaskTypes_t;

} // namespace ruth

#endif
