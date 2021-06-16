/*
  Ruth
  (C)opyright 2021  Tim Hughey

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

#include <string.h>

#include <esp_log.h>

#include "filter.hpp"
#include "network.hpp"

namespace message {

#pragma message "array subscript access to levels not supported for outbound filters"

IRAM_ATTR Filter::Filter(char report) {
  addLevel(_first_level);

  if (report != 0x00) {
    // add the report filter
    addLevelSeparator();
    *_next++ = report;
    _capacity--;
  }

  appendHostId();
}

IRAM_ATTR Filter::Filter(const char *topic, const size_t len) {
  split(topic, len);

  // prevent append
  _capacity = 0;
  _next = _filter + _max_capacity;
  dump();
}

IRAM_ATTR Filter::Filter(const Filter &rhs) {
  split(rhs._filter, _max_capacity);

  _capacity = 0;
  _next = _filter + _max_capacity;
  dump();
}

IRAM_ATTR void Filter::append(const char *filter) {
  // when called with a nullptr, do nothing
  if (filter == nullptr) return;

  if (_capacity == 0) return;

  addLevelSeparator();
  addLevel(filter);
}

IRAM_ATTR void Filter::addLevel(const char *filter) {
  if (_capacity == 0) return;

  // record a pointer to this newly added level for array subscript access
  _levels[_level_count++] = _next;

  char *p = _next;
  p = (char *)memccpy(p, filter, 0x00, _capacity);

  // memccpy returns a pointer to just after the copied null so back up one byte
  p--;

  // reduce capacity by what was placed into filter
  _capacity -= p - _next;

  // set _next for the next append
  _next = p;
}

IRAM_ATTR void Filter::appendHostId() {
  if (_capacity == 0) return;

  addLevelSeparator();
  addLevel(ruth::Net::hostID());
}

IRAM_ATTR void Filter::appendHostName() {
  if (_capacity == 0) return;

  addLevelSeparator();
  addLevel(ruth::Net::hostname());
}

IRAM_ATTR void Filter::dump() {
  ESP_LOGI("Filter", "length: %u capacity: %u", length(), availableCapacity());

  for (auto i = 0; i < _level_count; i++) {
    ESP_LOGI("Filter", "  level[%u] %s", i, _levels[i]);
  }
}

IRAM_ATTR void Filter::split(const char *topic, const size_t length) {
  // 1. make a copy of the event topic (filter) and null terminate
  // 2. search the filter for the level separator
  // 3. when each level separator is found record the starting pointer and replace
  //    the '/' with a null to terminate the string
  // 4. at the conclusion we have an array of pointers to the filter levels within the
  //    copy of the event topic

  memccpy(_filter, topic, 0x00, _max_capacity);
  _filter[length + 1] = 0x00;

  // set search used to find levels 0..4 (max of 5)
  auto search = _filter;
  // end points to the added null terminator
  auto const end = search + length + 1;
  constexpr size_t max_levels = sizeof(_levels) / sizeof(char *);

  // find each separator, record the found level and null the separator to
  // to create each individual string
  while ((_level_count < max_levels) && (search < end)) {
    auto separator = strchr(search, '/');

    // record the found level if it's not the end of the filter
    if (search != 0x00) {
      _levels[_level_count++] = search;
    }

    // reached the end of the filter, stop searching
    if (separator == nullptr) break;

    // null the separator, advance search to continue finding levels
    *separator = 0x00;
    search += separator - search + 1;
  }

  auto used = strnlen(_filter, _max_capacity);
  _capacity -= used;
  _next += used;
}

DRAM_ATTR const char *Filter::_first_level = nullptr;

} // namespace message
