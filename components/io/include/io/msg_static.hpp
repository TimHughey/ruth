/*
    Ruth
    Copyright (C) 2022  Tim Hughey

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

#include "io/io.hpp"
#include "misc/elapsed.hpp"
#include "ru_base/time.hpp"
#include "ru_base/types.hpp"
#include "ru_base/uint8v.hpp"

#include <ArduinoJson.h>
#include <arpa/inet.h>
#include <array>
#include <esp_log.h>
#include <memory>

namespace ruth {
namespace io {

static constexpr size_t DOC_DEFAULT_MAX_SIZE = 512;
static constexpr size_t MSG_LEN_SIZE = sizeof(uint16_t);
static constexpr size_t PACKED_DEFAULT_MAX_SIZE = DOC_DEFAULT_MAX_SIZE / 2;

using StaticPacked = std::array<char, PACKED_DEFAULT_MAX_SIZE>;
using StaticDoc = StaticJsonDocument<DOC_DEFAULT_MAX_SIZE>;

static constexpr ccs ASYNC_US = "async_µs";
static constexpr ccs DATA_PORT = "data_port";
static constexpr ccs DFRAME = "dframe";
static constexpr ccs ECHOED_NOW_US = "echoed_now_µs";
static constexpr ccs ELAPSED_US = "elapsed_µs";
static constexpr ccs FEEDBACK = "feedback";
static constexpr ccs FPS = "fps";
static constexpr ccs HANDSHAKE = "handshake";
static constexpr ccs IDLE_SHUTDOWN_US = "idle_shutdown_µs";
static constexpr ccs JITTER_US = "jitter_µs";
static constexpr ccs MAGIC = "magic";
static constexpr ccs NOW_US = "now_µs";
static constexpr ccs READ_MSG = "read_msg";
static constexpr ccs REF_US = "ref_µs";
static constexpr ccs SEQ_NUM = "seq_num";
static constexpr ccs SYNC_WAIT_US = "sync_wait_µs";
static constexpr ccs TYPE = "type";
static constexpr uint16_t MAGIC_VAL = 0xc9d2;

class Msg {
public:
  // outbound messages
  inline Msg(csv type, StaticPacked &packed) : type(type.data(), type.size()), packed(packed) {
    doc[TYPE] = type;
  }

  // inbound messages
  inline Msg(StaticPacked &packed) : type(io::READ_MSG), packed(packed) {}

  Msg(Msg &m) = delete;
  Msg(const Msg &m) = delete;
  inline Msg(Msg &&m) = default;

  inline void add_kv(csv key, auto val) { doc[key] = val; }

  // for Msg RX
  inline auto buff_msg_len() { return asio::buffer(packed.data(), MSG_LEN_SIZE); }

  // for Msg RX
  inline auto buff_packed() {
    auto *p = reinterpret_cast<uint16_t *>(packed.data());
    packed_len = ntohs(*p);

    return asio::buffer(packed.data(), packed_len);
  }

  // for Msg TX
  inline auto buff_tx() {
    uint16_t msg_len = htons(packed_len);
    char *p = reinterpret_cast<char *>(&msg_len);

    packed[0] = *p++;
    packed[1] = *p;

    tx_len = packed_len + MSG_LEN_SIZE;

    return asio::buffer(packed.data(), tx_len);
  }

  inline auto deserialize(error_code ec, size_t bytes) {
    const auto err = deserializeMsgPack(doc, packed.data(), packed.size());

    log_rx(ec, bytes, err);

    return !err;
  }

  inline bool can_render() const { return doc[MAGIC].as<int64_t>() == MAGIC_VAL; }

  template <typename T> inline T dframe() const {
    if (auto array = doc[DFRAME].as<JsonArrayConst>(); array) {
      return T(array);
    }

    return T(0);
  }

  inline auto key_equal(csv key, csv val) const { return val == doc[key]; }

  inline auto serialize() {
    doc[NOW_US] = rut::now_epoch<Micros>().count();
    doc[MAGIC] = MAGIC_VAL; // add magic as final key (to confirm complete msg)

    // NOTE:  reserve the first two bytes for the message length
    packed_len = serializeMsgPack(doc,                          //
                                  packed.data() + MSG_LEN_SIZE, // leave space for msg_len
                                  packed.size() - MSG_LEN_SIZE  //
    );
  }

  // misc logging, debug
  inline error_code log_rx(const error_code ec, const size_t bytes, const auto err) {
    if (ec || (packed_len != bytes) || err) {
      ESP_LOGW(module_id.data(), "%s failed, bytes=%d/%d reason=%s deserialize=%s", type.c_str(),
               bytes, packed_len, ec.message().c_str(), err.c_str());
    }

    return ec;
  }

  inline error_code log_tx(const error_code ec, const size_t bytes) {
    if (ec || (tx_len != bytes)) {
      ESP_LOGW(module_id.data(), "%s failed, bytes=%d/%d reason=%s", type.c_str(), bytes, tx_len,
               ec.message().c_str());
    }

    return ec;
  }

  // order dependent
  string type;
  StaticPacked &packed;
  StaticDoc doc;

  // order independent
  size_t packed_len = 0;
  size_t tx_len = 0;

  // misc debug
  static constexpr csv module_id{"io::Msg"};
};

} // namespace io
} // namespace ruth