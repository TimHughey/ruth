
//  Ruth
//  Copyright (C) 2022  Tim Hughey
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

static constexpr size_t DOC_DEFAULT_MAX_SIZE{640};
static constexpr size_t MSG_LEN_SIZE{sizeof(uint16_t)};
static constexpr size_t PACKED_DEFAULT_MAX_SIZE{DOC_DEFAULT_MAX_SIZE / 2};

using StaticPacked = std::array<char, PACKED_DEFAULT_MAX_SIZE>;
using StaticDoc = StaticJsonDocument<DOC_DEFAULT_MAX_SIZE>;

static constexpr auto DATA_PORT{"data_port"};
static constexpr auto DATA_WAIT_US{"data_wait_µs"};
static constexpr auto DFRAME{"dframe"};
static constexpr auto DMX_QOK{"dmx_qok"}; // dmx queue operation ok
static constexpr auto DMX_QRF{"dmx_qrf"}; // dmx queue recv failure count
static constexpr auto DMX_QSF{"dmx_qsf"}; // dmx queue send failure count
static constexpr auto ECHO_NOW_US{"echo_now_µs"};
static constexpr auto ELAPSED_US{"elapsed_µs"};
static constexpr auto FEEDBACK{"feedback"};
static constexpr auto FPS{"fps"};
static constexpr auto HANDSHAKE{"handshake"};
static constexpr auto IDLE_SHUTDOWN_MS{"idle_shutdown_ms"};
static constexpr auto MAGIC{"magic"};
static constexpr auto NOW_US{"now_µs"};
static constexpr auto READ_MSG{"read_msg"};
static constexpr auto REF_US{"ref_µs"};
static constexpr auto SEQ_NUM{"seq_num"};
static constexpr auto SHUTDOWN{"shutdown"};
static constexpr auto STATS_MS{"stats_ms"};
static constexpr auto TYPE{"type"};
static constexpr uint16_t MAGIC_VAL{0xc9d2};

class Msg {
public:
  // outbound messages
  inline Msg(csv type, StaticPacked &packed) noexcept
      : type(type.data()), //
        packed(packed)     //
  {
    doc[TYPE] = type;
  }

  // inbound messages
  inline Msg(StaticPacked &packed) noexcept : type(io::READ_MSG), packed(packed) {}

  Msg(Msg &m) = delete;
  Msg(const Msg &m) = delete;
  inline Msg(Msg &&m) = default;

  template <typename T> inline void add_kv(csv key, T val) {

    if constexpr (std::is_same_v<T, Elapsed>) {
      doc[key] = val().count();
    } else if constexpr (std::is_same_v<T, Nanos> || std::is_same_v<T, Micros> ||
                         std::is_same_v<T, Millis>) {
      doc[key] = val.count();
    } else {
      doc[key] = val;
    }
  }

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

  inline bool can_render() const noexcept { return doc[io::MAGIC] == MAGIC_VAL; }

  inline auto deserialize(error_code ec, size_t bytes) {
    const auto err = deserializeMsgPack(doc, packed.data(), packed.size());

    log_rx(ec, bytes, err);

    return !err;
  }

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
      ESP_LOGW(module_id.data(), "%s bytes=%d/%d %s %s", type.c_str(), bytes, packed_len,
               ec.message().c_str(), err.c_str());
    }

    return ec;
  }

  inline error_code log_tx(const error_code ec, const size_t bytes) {
    if (ec || (tx_len != bytes)) {
      ESP_LOGW(module_id.data(), "%s bytes=%d/%d %s", type.c_str(), bytes, tx_len,
               ec.message().c_str());
    }

    return ec;
  }

  // order dependent
  string type;
  StaticPacked &packed;
  StaticDoc doc;

  // order independent
  size_t packed_len{0};
  size_t tx_len{0};

  // misc debug
  static constexpr csv module_id{"io::Msg"};
};

} // namespace io
} // namespace ruth