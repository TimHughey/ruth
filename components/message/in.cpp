
//  Ruth
//  Copyright (C) 2021  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com
#include "message/in.hpp"

#include <esp_attr.h>
#include <esp_log.h>

namespace ruth {
namespace message {

static const char *TAG = "In";

In::In(const char *filter, const size_t filter_len, const char *packed, const size_t packed_len)
    : _filter(filter, filter_len), _packed_len(packed_len) {
  _packed = std::make_unique<char[]>(packed_len);

  memcpy(_packed.get(), packed, packed_len);
}

void In::checkTime(JsonDocument &root) {

  struct timeval time_now {};
  gettimeofday(&time_now, nullptr);

  uint64_t now_ms = ((uint64_t)time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
  auto mtime = root["mtime"].as<uint64_t>() | 0;

  if (mtime == 0) {
    ESP_LOGI(TAG, "mtime == 0");
    _valid = false;
    return;
  }

  _valid = (mtime > (now_ms - 1000)) ? true : false;

  if (_valid) return;

  ESP_LOGI(TAG, "mtime variance[%llu]", (mtime > now_ms) ? (mtime - now_ms) : (now_ms - mtime));
}

InWrapped In::make(const char *filter, const size_t filter_len, const char *packed,
                   const size_t packed_len) {
  return std::make_unique<In>(filter, filter_len, packed, packed_len);
}

bool In::unpack(JsonDocument &doc) {
  doc.clear();

  _err = deserializeMsgPack(doc, _packed.get(), _packed_len);

  if (_err) {
    ESP_LOGW(TAG, "deserialization error: %s", _err.c_str());
    _valid = false;
    return _valid;
  }

  checkTime(doc);

  return _valid;
}

} // namespace message
} // namespace ruth
