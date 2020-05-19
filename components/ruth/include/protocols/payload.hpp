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
#include "misc/elapsedMillis.hpp"
#include "misc/local_types.hpp"
#include "net/network.hpp"

namespace ruth {
using std::unique_ptr;
using std::vector;

typedef class MsgPayload MsgPayload_t;

typedef unique_ptr<MsgPayload_t> MsgPayload_t_ptr;

class MsgPayload {
public:
  MsgPayload(struct mg_str *in_topic, struct mg_str *in_payload) {
    parseTopic(in_topic);

    _data.reserve(in_payload->len + 2);
    _data.assign(in_payload->p, (in_payload->p + in_payload->len));
    _data.push_back(0x00); // ensure null termination
  };

  const vector<char> &data() const { return _data; }
  bool emptyPayload() const { return _data.empty(); }
  bool forThisHost() const {
    return (_host.compare(Net::hostID()) == 0 ? true : false);
  }

  const string_t &host() const { return _host; }
  bool hasSubtopic() const { return _has_subtopic; }
  void logElapsed() {
    _elapsed.freeze();
    ESP_LOGI("Payload", "elapsed=%0.3fms", (float)_elapsed / 1000.0);
  }
  bool matchSubtopic(const char *subtopic) const {
    return (_subtopic.compare(subtopic) == 0 ? true : false);
  }
  const char *payload() const { return _data.data(); }
  const string_t &subtopic() const { return _subtopic; }

private:
  bool _has_subtopic = false;
  vector<char> _data;

  string_t _host;
  string_t _subtopic;

  elapsedMicros _elapsed;

  void parseTopic(struct mg_str *in_topic) {
    string_t topic(in_topic->p, in_topic->len);

    // find the positions of the slashes
    // prod/<host>/<subtopic>
    auto slash1 = topic.find_first_of('/');

    // bad topic format
    if (slash1 == string_t::npos)
      return;

    auto slash2 = topic.find_first_of('/', slash1 + 1);

    // bad topic format
    if (slash2 == string_t::npos)
      return;

    auto more_slashes =
        (topic.find_first_of('/', slash2 + 1) == string_t::npos) ? false : true;

    if (more_slashes)
      return;

    // ok, the topic format meets expectations

    auto host_spos = slash1 + 1;
    auto host_epos = slash2 - 1;
    auto host_len = host_epos - host_spos;
    auto subtopic_spos = slash2 + 1;

    _host.assign(topic, host_spos, host_len);
    _subtopic.assign(topic, subtopic_spos);

    // success, mark this payload valid
    _has_subtopic = true;

    ESP_LOGV("Payload", "host=\"%s\" subtopic=\"%s\"", _host.c_str(),
             _subtopic.c_str());
  }
};
} // namespace ruth
#endif
