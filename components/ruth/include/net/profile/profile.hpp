/*
    profile.hpp -- Profile
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

#ifndef _ruth_profile_hpp
#define _ruth_profile_hpp

#include <algorithm>
#include <cstdlib>

#include <sys/time.h>
#include <time.h>

#include "local/types.hpp"
#include "misc/elapsed.hpp"
#include "net/profile/engine.hpp"
#include "net/profile/types.hpp"
#include "protocols/payload.hpp"

namespace ruth {

using std::find_if;
using std::unique_ptr;

typedef class Profile Profile_t;
typedef class TextBuffer<40> Version_t;
typedef class TextBuffer<20> ProfileName_t;

class Profile {
public:
  Profile(){};

  static bool postParseActions() { return _instance_()._postParseActions(); };

  static const char *assignedName() {
    return _instance_()._assigned_name.c_str();
  };

  static uint32_t coreLoopTicks() {
    return pdMS_TO_TICKS(_instance_()._core_loop_ms);
  }

  // was the Profile received within the previous 60 seconds
  static bool current() {
    return (_instance_()._mtime > (time(nullptr) - 60)) ? true : false;
  }

  static bool engineEnabled(EngineTypes_t engine_type) {
    return _instance_()._engine_enabled[engine_type];
  }

  static TickType_t engineTaskIntervalTicks(EngineTypes_t engine_type,
                                            EngineTaskTypes_t task_type);

  static uint32_t engineTaskPriority(EngineTypes_t engine_type,
                                     EngineTaskTypes_t task_type);

  static uint32_t engineTaskStack(EngineTypes_t engine_type,
                                  EngineTaskTypes_t task_type);

  static void fromRaw(MsgPayload_t *payload) {
    _instance_()._fromRaw(payload);
  };

  // MISC
  static bool i2cMultiplexer() { return _instance_()._i2c_mplex; };

  static const char *profileName() {
    return _instance_()._profile_name.c_str();
  };

  static uint64_t timestampMS() { return _instance_()._core_timestamp_ms; }

  static bool valid() { return _instance_()._valid; }

  static const char *version() { return _instance_()._version.c_str(); };

  static bool watchStacks() { return _instance_()._watch_stacks; }

private:
  Profile(MsgPayload_t *payload); // SINGLETON!  constructor is private

  void _fromRaw(MsgPayload_t *payload);
  static Profile_t &_instance_();

  bool _postParseActions();

private:
  elapsedMicros _parse_elapsed;
  DeserializationError _parse_err;
  bool _valid = false;

  // root data
  Hostname_t _assigned_name;
  time_t _mtime = 0;

  // metadata
  // 00.00.10-12-ge435da3f-dirty
  Version_t _version;
  ProfileName_t _profile_name;

  // core task
  bool _watch_stacks = false;
  TickType_t _core_loop_ms = 1000;
  uint32_t _core_timestamp_ms = 60 * 6 * 1000;

  // misc
  bool _i2c_mplex = false;

  // array of engine enabled
  bool _engine_enabled[ENGINE_END_OF_LIST];

  // 2D array of engine tasks
  ProfileEngineTask_t _engine_tasks[EngineTypes_t::ENGINE_END_OF_LIST]
                                   [EngineTaskTypes_t::TASK_END_OF_LIST];
};

} // namespace ruth

#endif
