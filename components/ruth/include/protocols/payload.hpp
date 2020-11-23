/*
    mqtt/payload.hpp - Ruth MQTT Payload
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

#ifndef _ruth_mqtt_payload_hpp
#define _ruth_mqtt_payload_hpp

#include <cstdlib>
#include <mqtt_client.h>

#include "net/network.hpp"

namespace ruth {
using std::unique_ptr;

typedef class TextBuffer<25> Topic_t;
typedef class TextBuffer<64> TopicErrMsg_t;
typedef class TextBuffer<768> RawPayload_t;
typedef class MsgPayload MsgPayload_t;

typedef unique_ptr<MsgPayload_t> MsgPayload_t_ptr;

class MsgPayload {
public:
  //
  // NOTE:
  //  although many of the implementations for the class functions
  //  are trivial they have been placed in a separate .cpp file to
  //  minimize aggressive inlining and code bloat
  //

  MsgPayload(esp_mqtt_event_t *event);
  MsgPayload(const MsgPayload &s) = delete; // no copies!
  ~MsgPayload();

  // check validity and access the topic that failed validation
  bool valid();
  bool invalid();
  const char *errorTopic() const;

  // payload data functionality
  const char *payload() const;
  bool emptyPayload() const;

  // topic host functionality
  bool forThisHost() const;
  const char *host() const;

  // subtopic functionality
  bool hasSubtopic();
  bool matchSubtopic(const char *match) const;

  size_t length() const { return _data.size(); }

  const char *subtopic() const;

  // topic mtime functionality
  bool current();

private:
  typedef enum {
    PART_ENV = 0,
    PART_HOST,
    PART_SUBTOPIC,
    PART_MTIME,
    PART_END_OF_LIST
  } TopicParts_t;

  bool _has_part[PART_END_OF_LIST] = {};
  time_t _mtime = 0;

  RawPayload_t _data;

  static const size_t _max_parts = PART_END_OF_LIST;
  Topic_t _topic_parts[_max_parts];

  TopicErrMsg_t _err_topic;

  // parse out subtopics using slashes in topic
  void parseTopic(esp_mqtt_event_t *event);
  // validate and determine if the expected subtopics are present
  void validateSubtopics();
};
} // namespace ruth
#endif
