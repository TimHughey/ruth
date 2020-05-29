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
#include "readings/simple_text.hpp"

namespace ruth {

MsgPayload::MsgPayload(struct mg_str *in_topic, struct mg_str *in_payload) {
  parseTopic(in_topic);
  validateSubtopics();

  // _data.reserve(in_payload->len + 2);
  _data.assign(in_payload->p, (in_payload->p + in_payload->len));
  // _data.push_back(0x00); // ensure null termination

  if (invalid() && _err_topic.empty()) {
    _err_topic.assign(in_topic->p, in_topic->len);
  }
}

MsgPayload::~MsgPayload() {
  for (auto i = 0; i < _topic_parts.size(); i++) {
    delete _topic_parts.at(i);
  }
}

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
const vector<char> &MsgPayload::data() const { return _data; }
const char *MsgPayload::payload() const { return _data.data(); }
bool MsgPayload::emptyPayload() const { return _data.empty(); }

// topic host functionality
bool MsgPayload::forThisHost() const {
  return (_topic_parts.at(PART_HOST)->compare(Net::hostID()) == 0 ? true
                                                                  : false);
}

const string_t &MsgPayload::host() const {
  return *(_topic_parts.at(PART_HOST));
}

// subtopic functionality
bool MsgPayload::hasSubtopic() {
  auto has_subtopic = _has_part[PART_SUBTOPIC];

  if (has_subtopic)
    return true;

  auto max_len = 64;
  unique_ptr<char[]> buf(new char[max_len + 1]);

  snprintf(buf.get(), max_len, "payload topic does not have a subtopic");
  _err_topic = buf.get();

  return false;
}

bool MsgPayload::matchSubtopic(const char *match) const {
  return (_topic_parts.at(PART_SUBTOPIC)->compare(match) == 0 ? true : false);
}

const string_t &MsgPayload::subtopic() const {
  return *_topic_parts.at(PART_SUBTOPIC);
}
const char *MsgPayload::subtopic_c() const {
  return _topic_parts.at(PART_SUBTOPIC)->c_str();
}

// topic mtime functionality
bool MsgPayload::current() {
  auto now = time(nullptr);

  auto recent = (_mtime > (now - 60)) ? true : false;

  if (recent)
    return true;

  auto max_len = 256;
  unique_ptr<char[]> buf(new char[max_len + 1]);

  auto diff = now - _mtime;

  snprintf(buf.get(), max_len, "payload mtime failure, diff=%ld", diff);

  _err_topic = buf.get();

  return false;
}

// use the slashes in the topic to parse out the subtopics
void MsgPayload::parseTopic(struct mg_str *in_topic) {
  static const size_t max_parts = sizeof(_topic_parts);

  // i is the index into the topic vector
  // spos is the starting position of the part found (starting at zero)
  for (size_t i = 0, spos = 0; i <= in_topic->len; i++) {
    // part is found when either:
    //  a. slash is detected
    //  b. the end of the topic is reached
    if ((in_topic->p[i] == '/') || (i == in_topic->len)) {
      // slash was found, compute the length of the part
      //  a. position of the slash (i)
      //  b  subtract the starting position (spos)
      //  c. minus one to account for the slash
      //
      //  * the last part is from the previous slash to end of topic

      // construct the string from the starting position (spos) and length
      const size_t len = i - spos;
      const string_t *part = new string_t(&(in_topic->p[spos]), len);

      _topic_parts.push_back(part);

      spos += len + 1; // the next starting position skips the slash
    }

    // limit the number of parts to find
    if (_topic_parts.size() >= max_parts) {
      break;
    }
  }
}

// validate and determine if the expected subtopics are present
void MsgPayload::validateSubtopics() {

  // exmaple:
  //  index: 0....1......2..........3......
  //  topic: prod/<host>/<subtopic>/<mtime>
  for (uint32_t i = 0; i < _topic_parts.size(); i++) {
    const string_t *part = _topic_parts.at(i);

    if ((TopicParts_t)i < PART_END_OF_LIST) {
      _has_part[i] = (part->empty()) ? false : true;
    }
  }

  if (_has_part[PART_MTIME]) {
    _mtime = atoll(_topic_parts.at(PART_MTIME)->c_str());
  }
}

} // namespace ruth
