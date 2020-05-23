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

#ifndef ruth_profile_hpp
#define ruth_profile_hpp

#include <algorithm>
#include <cstdlib>
#include <vector>

#include <sys/time.h>
#include <time.h>

#include "misc/elapsedMillis.hpp"
#include "misc/local_types.hpp"
#include "net/profile/engine.hpp"
#include "net/profile/types.hpp"
#include "protocols/payload.hpp"

namespace ruth {

using std::find_if;
using std::unique_ptr;
using std::vector;

typedef class Profile Profile_t;

class Profile {
public:
  static void postParseActions() { _instance_()->_postParseActions(); };

  static const char *assignedName() {
    return _instance_()->_assigned_name.c_str();
  };

  static uint32_t coreLoopTicks() {

    return (_instance_()) ? pdMS_TO_TICKS(_instance_()->_core_loop_ms)
                          : pdMS_TO_TICKS(1000);
  }

  // was the Profile received within the previous 60 seconds
  static bool current() {
    return (_instance_()->_mtime > (time(nullptr) - 60)) ? true : false;
  }

  static bool engineEnabled(EngineTypes_t engine_type) {
    return _instance_()->_engine_enabled[engine_type];
  }

  static TickType_t engineTaskIntervalTicks(EngineTypes_t engine_type,
                                            EngineTaskTypes_t task_type);

  static uint32_t engineTaskPriority(EngineTypes_t engine_type,
                                     EngineTaskTypes_t task_type);

  static uint32_t engineTaskStack(EngineTypes_t engine_type,
                                  EngineTaskTypes_t task_type);

  static void fromRaw(MsgPayload_t *payload) { Profile::_fromRaw(payload); };

  static bool i2cMultiplexer() { return _instance_()->_i2c_mplex; };

  static const char *profileName() {
    return _instance_()->_profile_name.c_str();
  };

  static uint32_t timestampMS() { return _instance_()->_core_timestamp_ms; }

  static bool valid() { return (_instance_()) ? _instance_()->_valid : false; }

  static const char *version() { return _instance_()->_version.c_str(); };

private:
  Profile(MsgPayload_t *payload); // SINGLETON!  constructor is private

  static void _fromRaw(MsgPayload_t *payload);
  static Profile_t *_instance_();

  void _postParseActions();

private:
  elapsedMicros _parse_elapsed;
  DeserializationError _parse_err;
  bool _valid = false;

  // root data
  string_t _assigned_name;
  time_t _mtime = 0;

  // metadata
  string_t _version;
  string_t _profile_name;

  // core task
  bool _watch_stacks = false;
  TickType_t _core_loop_ms = 1000;
  uint32_t _core_timestamp_ms = 60 * 6 * 1000;

  // misc
  bool _i2c_mplex = false;

  // array of engine enabled
  bool _engine_enabled[ENGINE_END_OF_LIST];

  // vector of specific engines profile info
  vector<ProfileEngineTask_t *> _engine_tasks;
};

} // namespace ruth

#endif
