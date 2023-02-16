/*
  Message
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

#pragma once

#include "filter/out.hpp"

#include <ArduinoJson.h>
#include <memory>

namespace ruth {
namespace message {

typedef std::unique_ptr<char[]> Packed;

class Out {
public:
  Out(size_t doc_size = 1024) noexcept;
  virtual ~Out() = default;

  inline JsonDocument &doc() { return _doc; }
  inline const char *filter() const { return _filter.c_str(); }
  inline size_t memoryUsage() const { return _doc.memoryUsage(); }
  Packed pack(size_t &length);
  inline uint32_t qos() const { return _qos; }
  inline JsonObject rootObject() { return _doc.as<JsonObject>(); }

private:
  virtual void assembleData(JsonObject &rootObject) = 0;

protected:
  filter::Out _filter;

private:
  DynamicJsonDocument _doc;
  uint32_t _qos = 0;
};

} // namespace message
} // namespace ruth
