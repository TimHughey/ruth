/*
    profile.cpp -- Profile
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

#include <cstdlib>

#ifdef LINTER
#define MBEDTLS_CONFIG_FILE "mbedtls/esp_config.h"
#endif

#include <mbedtls/base64.h>

#include "core/binder.hpp"
#include "net/network.hpp"
#include "net/profile/profile.hpp"

namespace ruth {

using PET = ProfileEngineTask;
using PET_t = ProfileEngineTask_t;

static Profile_t singleton;

static const bool _unset_bool = false;

// static
uint32_t Profile::engineTaskInterval(EngineTypes_t engine_type,
                                     EngineTaskTypes_t task_type) {

  const PET_t &pet = i()._engine_tasks[engine_type][task_type];

  return pet.intervalMS();
}

// static
TickType_t Profile::engineTaskIntervalTicks(EngineTypes_t engine_type,
                                            EngineTaskTypes_t task_type) {

  const PET_t &pet = i()._engine_tasks[engine_type][task_type];

  return pdMS_TO_TICKS(pet.intervalMS());
}

// static
uint32_t Profile::engineTaskPriority(EngineTypes_t engine_type,
                                     EngineTaskTypes_t task_type) {

  const PET_t &pet = i()._engine_tasks[engine_type][task_type];

  return pet.priority();
}

// static
uint32_t Profile::engineTaskStack(EngineTypes_t engine_type,
                                  EngineTaskTypes_t task_type) {

  const PET_t &pet = i()._engine_tasks[engine_type][task_type];

  return pet.stackSize();
}

//
// PRIVATE
//

bool Profile::_postParseActions() {
  Net::setName(_assigned_name.c_str());
  return true;
}

void Profile::_fromRaw(MsgPayload_t *payload) {
  StaticJsonDocument<_doc_capacity> root;

  _parse_elapsed.reset();
  _parse_err = deserializeMsgPack(root, payload->payload(), payload->length());
  _parse_elapsed.freeze();

  if (_parse_err) {
    ESP_LOGW("Profile", "parse failed %s", _parse_err.c_str());
    return;
  }

  auto used = root.memoryUsage();
  auto used_percent = ((float)used / (float)_doc_capacity) * 100.0;
  ESP_LOGI("Profile", "JsonDocument usage[%0.1f%%] (%u/%u)", used_percent, used,
           _doc_capacity);

  _valid = true;

  const JsonObject meta = root["meta"];
  const JsonObject core = root["core"];
  const JsonObject lightdesk = root["lightdesk"];
  const JsonObject misc = root["misc"];

  _assigned_name = root["assigned_name"] | "none";

  _mtime = root["mtime"] | _mtime;

  _profile_name = meta["profile_name"] | "none";

  _version = meta["version"] | "none";

  _watch_stacks = misc["watch_stacks"] | _watch_stacks;
  _core_loop_ms = core["loop_ms"] | _core_loop_ms;
  _core_timestamp_ms = core["timestamp_ms"] | _core_timestamp_ms;

  _lightdesk_enabled = lightdesk["enable"] | false;

  _i2c_mplex = misc["i2c_mplex"] | _i2c_mplex;

  // the two loops will create and copy a PET into the 2D array containing
  // all combinations of Engines and Engine Tasks.  NOTE:  this is a
  // flat list so finding the proper entry requires comparing EngineType and
  // EngineTaskType

  // loops through all the possible engine types
  for (int e = ENGINE_DALSEMI; e < ENGINE_END_OF_LIST; e++) {
    const char *engine_key = PET::lookupEngineKey((EngineTypes_t)e);

    const JsonObject engine_doc = root[engine_key];

    // capture if the engine is enabled, defaulting to false
    _engine_enabled[e] = engine_doc["enable"] | false;

    // loops through all the possible task types
    for (int et = TASK_CORE; et < TASK_END_OF_LIST; et++) {
      const char *task_key = PET::lookupTaskKey((EngineTaskTypes_t)et);

      if (engine_doc.containsKey(task_key)) {

        PET_t task((EngineTypes_t)e, (EngineTaskTypes_t)et, engine_doc);

        _engine_tasks[e][et] = task;
      }
    }
  }
}

// STATIC
Profile_t &Profile::i() { return singleton; }
} // namespace ruth
