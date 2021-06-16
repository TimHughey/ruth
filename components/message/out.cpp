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

#include <ctime>
#include <sys/time.h>

#include <esp_attr.h>

#include "out.hpp"

namespace message {

IRAM_ATTR Out::Out(const size_t doc_size) : _filter('r'), _doc(doc_size) {}

IRAM_ATTR Packed Out::pack(size_t &length) {
  JsonObject data = _doc.to<JsonObject>();

  struct timeval time_now {};
  gettimeofday(&time_now, nullptr);

  uint64_t mtime_ms = ((uint64_t)time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);

  data["mtime"] = mtime_ms;

  assembleData(data);

  auto packed_size = measureMsgPack(_doc);

  auto packed = std::make_unique<char[]>(packed_size);

  length = serializeMsgPack(_doc, packed.get(), packed_size);

  return std::move(packed);
}

} // namespace message
