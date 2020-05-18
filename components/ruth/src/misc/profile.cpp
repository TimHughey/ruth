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

#include "misc/profile.hpp"
#include "net/network.hpp"

namespace ruth {

using ArduinoJson::DynamicJsonDocument;

static const char *TAG = "Profile";
static Profile_t *__singleton__ = nullptr;

// inclusive of largest profile document
static const size_t _doc_capacity =
    4 * JSON_OBJECT_SIZE(2) + 9 * JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) +
    2 * JSON_OBJECT_SIZE(5) + 2 * JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(9) +
    710;

static DynamicJsonDocument _doc(_doc_capacity);
static const char *_empty_string = "";
static const bool _unset_bool = false;

Profile::Profile() {}

// PRIVATE
const char *Profile::_assignedName() {
  return _doc.isNull() ? _empty_string : _doc["assigned_name"];
}

size_t Profile::_capacity() { return _doc.capacity(); }

bool Profile::_parseRawMsg(const char *raw) {

  _parse_elapsed.reset();

  // as of 2020-05-13 assume all messages are MsgPax encoded
  _parse_rc = deserializeMsgPack(_doc, raw);

  _parse_elapsed.freeze();

  return (_parse_rc) ? false : true;
}

void Profile::_postParseActions() {

  // ESP_LOGI(TAG, "msgpack deserialization took %lldus",
  //          (uint64_t)_parse_elapsed);
  //
  // ESP_LOGI(TAG, "using profile=\"%s\" version=\"%s\"", _profileName(),
  //          _version());

  Net::setName(_assignedName());
}

const char *Profile::_profileName() {
  return _doc.isNull() ? _empty_string
                       : _doc["meta"]["profile"].as<const char *>();
}

const char *Profile::_version() {
  return _doc.isNull() ? _empty_string
                       : _doc["meta"]["version"].as<const char *>();
}

// Access to Subsystem Keys

bool Profile::_subSystemEnable(const char *subsystem) {
  return _doc[subsystem]["enable"];
}

bool Profile::_subSystemBoolean(const char *subsystem, const char *key) {
  return _doc[subsystem][key];
}

uint32_t Profile::_subSystemUINT32(const char *subsystem, const char *key) {
  return _doc[subsystem][key].as<uint32_t>();
}

// Acces to Subsystem Task Keys
TickType_t Profile::_subSystemTaskInterval(const char *subsystem,
                                           const char *task) {
  const uint32_t secs = _doc[subsystem][task]["interval_secs"].as<uint32_t>();

  return pdMS_TO_TICKS(secs * 1000);
}

uint32_t Profile::_subSystemTaskPriority(const char *subsystem,
                                         const char *task) {
  return _doc[subsystem][task]["pri"].as<uint32_t>();
}

size_t Profile::_subSystemTaskStack(const char *subsystem, const char *task) {
  return _doc[subsystem][task]["stack"].as<size_t>();
}

// STATIC
Profile_t *Profile::_instance_() {
  if (__singleton__ == nullptr) {
    __singleton__ = new Profile();
  }

  return __singleton__;
}
} // namespace ruth
