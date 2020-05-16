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

#include <sys/time.h>
#include <time.h>

#include <esp_log.h>

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_system.h>

#include "external/ArduinoJson.h"
#include "misc/elapsedMillis.hpp"
#include "misc/local_types.hpp"

namespace ruth {

typedef class Profile Profile_t;

class Profile {
public:
  static const char *assignedName() { return _instance_()->_assignedName(); };
  static size_t capacity() { return _instance_()->_capacity(); };
  static bool parseRawMsg(rawMsg_t *raw) {
    return _instance_()->_parseRawMsg(raw);
  };
  static void postParseActions() { _instance_()->_postParseActions(); };
  static const char *profileName() { return _instance_()->_profileName(); };
  static const char *version() { return _instance_()->_version(); };

  // Generalized Subsystem Task Prioirty and Stack Access
  static TickType_t subSystemTaskInterval(const char *subsystem,
                                          const char *task) {
    return _instance_()->_subSystemTaskInterval(subsystem, task);
  }

  static uint32_t subSystemTaskPriority(const char *subsystem,
                                        const char *task) {
    return _instance_()->_subSystemTaskPriority(subsystem, task);
  }

  static size_t subSystemTaskStack(const char *subsystem, const char *task) {
    return _instance_()->_subSystemTaskStack(subsystem, task);
  }

  // DallasSemi
  static bool dalsemiEnable() { return _instance_()->_subSystemEnable("ds"); };

  // i2c
  static bool i2cEnable() { return _instance_()->_subSystemEnable("i2c"); };
  static bool i2cUseMultiplexer() {
    return _instance_()->_subSystemBoolean("i2c", "use_multiplexer");
  };

  // PWM
  static bool pwmEnable() { return _instance_()->_subSystemEnable("pwm"); };
  static uint32_t pwmTaskInterval(const char *task) {
    return _instance_()->_subSystemTaskInterval("pwm", task);
  };

  // timestamp
  static uint32_t timestampFrequencySecs() {
    return _instance_()->_subSystemUINT32("timestamp", "report_interval_secs");
  }

private:
  static Profile_t *_instance_();
  Profile(); // SINGLETON!  constructor is private

  const char *_assignedName();
  size_t _capacity();
  bool _parseRawMsg(rawMsg_t *raw);
  void _postParseActions();
  bool _subSystemEnable(const char *subsystem);
  bool _subSystemBoolean(const char *subsystem, const char *key);

  uint32_t _subSystemUINT32(const char *subsystem, const char *key);

  // subsystem task settings
  TickType_t _subSystemTaskInterval(const char *subsystem, const char *task);
  uint32_t _subSystemTaskPriority(const char *subsystem, const char *task);
  size_t _subSystemTaskStack(const char *subsystem, const char *task);

  const char *_profileName();
  const char *_version();

private:
  elapsedMicros _parse_elapsed;
  DeserializationError _parse_rc;

  // ArduinoJson::DynamicJsonDocument _doc(1024);
}; // namespace ruth

} // namespace ruth

#endif
