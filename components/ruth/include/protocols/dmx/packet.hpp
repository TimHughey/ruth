/*
    Ruth
    Copyright (C) 2021  Tim Hughey

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

#ifndef _ruth_dmx_net_packet_hpp
#define _ruth_dmx_net_packet_hpp

#include <asio.hpp>
#include <vector>

#include "core/binder.hpp"
#include "external/ArduinoJson.h"

namespace ruth {
namespace dmx {

typedef std::vector<uint8_t> Frame;

class Packet {
public:
  Packet() = default;

  inline bool deserializeMsg() {
    auto rc = true;
    auto err = deserializeMsgPack(doc, msg(), msgLength());

    if (err) {
      rc = false;
    }

    return rc;
  }

  inline uint8_t *frameData() const { return (uint8_t *)&(p.payload); }
  inline size_t frameDataLength() const { return p.len.frame; }
  inline bool validMagic() { return p.magic == Binder::dmxMagic(); }
  inline char *msg() { return p.payload + p.len.frame; };
  inline size_t msgLength() const { return p.len.msg; }

  inline JsonObject rootObj() { return doc.as<JsonObject>(); }

  inline uint8_t *rxData() { return (uint8_t *)&p; }
  size_t rxDataLength() const { return sizeof(p); }

public:
  Frame frame;

private:
  typedef StaticJsonDocument<384> MsgPackDoc;

private:
  MsgPackDoc doc;

  // packet contents wrapped to ensure contiguous memory
  struct {
    uint16_t magic = 0x00;
    struct {
      uint16_t packet = 0;
      uint16_t frame = 0;
      uint16_t msg = 0;
    } len;
    char payload[768] = {0};
  } p;
};

} // namespace dmx
} // namespace ruth

#endif
