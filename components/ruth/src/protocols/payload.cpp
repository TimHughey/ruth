/*
     protocols/payload.cpp - Ruth MQTT Payload
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

#include "protocols/payload.hpp"
#include "readings/text.hpp"

namespace ruth {

MsgPayload::MsgPayload(esp_mqtt_event_t *event) {
  parseTopic(event);
  validateSubtopics();

  _data.assign(event->data, event->data_len);

  if (invalid() && _err_topic.empty()) {
    _err_topic.assign(event->data, event->data_len);
  }
}

MsgPayload::MsgPayload(const char *subtopic) {
  _topic_parts[PART_SUBTOPIC].printf("%s", subtopic);
}

MsgPayload::MsgPayload(const void *data, size_t len) {
  _data.assign(static_cast<const char *>(data), len);
}

MsgPayload::~MsgPayload() {}

//
// NOTE:
//  these functions are small however placed here to prevent aggressive
//  inlining and code bloat
//

// check validity and access the topic that failed validation
bool MsgPayload::valid() { return (hasSubtopic() && current()); }
bool MsgPayload::invalid() { return !valid(); }
const char *MsgPayload::errorTopic() const { return _err_topic.c_str(); }

// payload data functionality
char *MsgPayload::data() { return _data.data(); }
const char *MsgPayload::payload() const { return _data.c_str(); }
bool MsgPayload::emptyPayload() const { return _data.empty(); }

// topic host functionality
bool MsgPayload::forThisHost() const {
  return _topic_parts[PART_HOST].match(Net::hostID());
}

const char *MsgPayload::host() const { return _topic_parts[PART_HOST].c_str(); }

// subtopic functionality
bool MsgPayload::hasSubtopic() {
  auto has_subtopic = _has_part[PART_SUBTOPIC];

  if (has_subtopic)
    return true;

  _err_topic = "payload topic does not have a subtopic";

  return false;
}

bool MsgPayload::matchSubtopic(const char *topic) const {
  return _topic_parts[PART_SUBTOPIC].match(topic);
}

const char *MsgPayload::subtopic() const {
  return _topic_parts[PART_SUBTOPIC].c_str();
}

// topic mtime functionality
bool MsgPayload::current() {
  auto now = time(nullptr);

  auto recent = (_mtime > (now - 60)) ? true : false;

  if (recent)
    return true;

  _err_topic.printf("mtime variance[%ld]", (now - _mtime));

  return false;
}

// use the slashes in the topic to parse out the subtopics
void MsgPayload::parseTopic(esp_mqtt_event_t *event) {
  auto found_parts = 0;

  // i is the index into the topic array
  // spos is the starting position of the part found (starting at zero)
  for (size_t i = 0, spos = 0; i <= event->topic_len; i++) {
    // part is found when either:
    //  a. slash is detected
    //  b. the end of the topic is reached
    if ((event->topic[i] == '/') || (i == event->topic_len)) {
      // slash was found, compute the length of the part
      //  a. position of the slash (i)
      //  b  subtract the starting position (spos)
      //  c. minus one to account for the slash
      //
      //  * the last part is from the previous slash to end of topic

      // construct the string from the starting position (spos) and length
      const size_t len = i - spos;
      const Topic_t part(&(event->topic[spos]), len);

      _topic_parts[found_parts++] = part;

      spos += len + 1; // the next starting position skips the slash
    }

    // limit the number of parts to find
    if (found_parts == _max_parts) {
      break;
    }
  }
}

// validate and determine if the expected subtopics are present
void MsgPayload::validateSubtopics() {

  // exmaple:
  //  index: 0....1......2..........3......
  //  topic: prod/<host>/<subtopic>/<mtime>
  for (uint32_t i = 0; i < _max_parts; i++) {
    const Topic_t &part = _topic_parts[i];

    if ((TopicParts_t)i < PART_END_OF_LIST) {
      _has_part[i] = (part.empty()) ? false : true;
    }
  }

  if (_has_part[PART_MTIME]) {
    _mtime = atoll(_topic_parts[PART_MTIME].c_str());
  }
}

} // namespace ruth
