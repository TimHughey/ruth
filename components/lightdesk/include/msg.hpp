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

#pragma once

#include "base/uint8v.hpp"
#include "io/io.hpp"

#include "ArduinoJson.h"
#include <memory>

namespace ruth {

class DeskMsg;
typedef std::shared_ptr<DeskMsg> shDeskMsg;

class DeskMsg : public std::enable_shared_from_this<DeskMsg> {

public:
  DeskMsg(std::array<char, 1024> &buff) {
    deserialize_ok = deserializeMsgPack(doc, buff.data()) ? false : true;

    if (deserialize_ok) {
      obj = doc.as<JsonObject>();
    }
  }

  inline bool good() const { return deserialize_ok; }

public:
  // defined early for auto
  inline JsonObjectConst root() const { return obj; }
  // shDeskMsg create() { return shDeskMsg(new DeskMsg); }
  // shDeskMsg ptr() { return shared_from_this(); }

  // auto buffer() { return asio::buffer(rx_buff.data(), rx_buff.size()); }

  // bool rxComplete(size_t bytes) {
  //   auto err = deserializeMsgPack(doc, rx_buff.raw<char>(), bytes);

  //   return err ? false : true;
  // }

  // uint8_t *frameData() const { return (uint8_t *)&(p.payload); }
  // size_t frameDataLength() const { return p.len.frame; }
  bool validMagic() const { return good() && ((obj[csv("magic")] | 0x00) == 0xc9d2); }

  // char *msg() { return p.payload + p.len.frame; };
  // size_t msgLength() const { return p.len.msg; }

  // uint8_t *rxData() { return (uint8_t *)&p; }
  // size_t rxMaxLen() const { return sizeof(p); }

private:
  typedef StaticJsonDocument<384> MsgPackDoc;

private:
  uint8v rx_buff;
  MsgPackDoc doc;
  JsonObject obj;

  bool deserialize_ok = false;

  // // packet contents wrapped to ensure contiguous memory
  // struct {
  //   uint16_t magic = 0x00;
  //   struct {
  //     uint16_t packet = 0;
  //     uint16_t frame = 0;
  //     uint16_t msg = 0;
  //   } len;
  //   char payload[768] = {};
  // } p;
};

} // namespace ruth
