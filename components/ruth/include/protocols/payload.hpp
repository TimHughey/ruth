/*
    types.hpp - Ruth MQTT Types
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

#ifndef ruth_mqtt_types_hpp
#define ruth_mqtt_types_hpp

#include <cstdlib>
#include <memory>
#include <string>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <sdkconfig.h>

#include "external/mongoose.h"
#include "misc/local_types.hpp"

namespace ruth {
using std::vector;

typedef class MsgPayload MsgPayload_t;

class MsgPayload {
public:
  MsgPayload(struct mg_str *in_topic, struct mg_str *in_payload) {
    _topic.assign(in_topic->p, in_topic->len);

    _data.reserve(in_payload->len + 2);
    _data.assign(in_payload->p, (in_payload->p + in_payload->len));
    _data.push_back(0x00); // ensure null termination
  };

  const vector<char> &data() const { return _data; }
  bool emptyPayload() const { return _data.empty(); }

  const char *payload() const { return _data.data(); }
  const string_t &topic() const { return _topic; }

private:
  string_t _topic;
  vector<char> _data;

  string_t _host;
  string_t _subtopic;
  string_t _device;
};
} // namespace ruth
#endif
