//  Pierre - Custom Light Show for Wiss Landing
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

#include "session/session.hpp"
#include "dmx/dmx.hpp"
#include "headunit/ac_power.hpp"
#include "headunit/discoball.hpp"
#include "headunit/elwire.hpp"
#include "headunit/headunit.hpp"
#include "headunit/ledforest.hpp"
#include "inject/inject.hpp"
#include "io/io.hpp"
#include "msg.hpp"
#include "ru_base/uint8v.hpp"

#include "ArduinoJson.h"
#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <chrono>
#include <cmath>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <mutex>
#include <vector>

namespace ruth {
namespace desk {

using namespace std::chrono_literals;
static constexpr csv TAG = "Session";

typedef std::vector<shHeadUnit> HeadUnits;
DRAM_ATTR static HeadUnits units;

namespace active {
DRAM_ATTR std::optional<Session> session;
} // namespace active

static void create_units() {
  units.emplace_back(std::make_unique<AcPower>("ac power"));
  units.emplace_back(std::make_unique<DiscoBall>("disco ball", 1)); // pwm 1
  units.emplace_back(std::make_unique<ElWire>("el dance", 2));      // pwm 2
  units.emplace_back(std::make_unique<ElWire>("el entry", 3));      // pwm 3
  units.emplace_back(std::make_unique<LedForest>("led forest", 4)); // pwm 4
}

// Object API

void IRAM_ATTR Session::data_msg_receive() {
  msg_len = 0;

  socket_data.async_receive( //
      asio::buffer(packed), [this, start_us = esp_timer_get_time()](error_code ec, size_t bytes) {
        // if no error, handle the incoming UDP data msg
        if (!ec) {
          const auto async_us = esp_timer_get_time() - start_us;

          // now that we have the entire packed message
          // attempt to create the DeskMsg, ask DMX to send the frame then
          // ask each headunit to handle it's part of the message
          if (auto msg = DeskMsg(packed, bytes, async_us); msg.playable()) {
            dmx->txFrame(msg.dframe<DMX::Frame>());

            for (auto &unit : units) {
              unit->handleMsg(msg.root());
            }
          }

          // we received and processed an actual data msg, reset the idle watch dog
          idle_watch_dog(); // reset the idle watchdog
          data_msg_receive();
        } else if (ec != io::aborted) {

          // idle_watch_dog() handles when data frames are missing
          // prepare for the next message (thereby ignoring the error) unless
          // the error is errc::operation_canceled (Session is ending)

          // NOTE:  if data_msg_receive() is not called we stop accepting
          //        data msgs

          ESP_LOGW(TAG.data(), "recv msg failed, bytes= %d reason=%s", bytes,
                   ec.message().c_str());

          data_msg_receive();
        }
      });
}

void IRAM_ATTR Session::handshake() {
  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();

  uint16_t data_port = socket_data.local_endpoint().port();
  root["data_port"] = data_port;

  if (send_ctrl_msg(doc)) {
    dmx = DMX::init(); // start DMX task

    data_msg_receive();
  } else {
    ESP_LOGW(TAG.data(), "handshake failed");

    shutdown();
  }
} // namespace desk

void IRAM_ATTR Session::idle_watch_dog() {
  auto expires = ru_time::as_duration<Seconds, Millis>(idle_shutdown);
  idle_timer.expires_after(expires);
  idle_timer.async_wait([this](const error_code ec) {
    // if the timer ever expires then we're idle
    if (!ec) {
      ESP_LOGI(TAG.data(), "idle timeout");

      std::for_each(units.begin(), units.end(), [](auto &unit) { unit->dark(); });

      shutdown();
    } else {
      ESP_LOGD(TAG.data(), "idleWatchDog() terminating reason=%s", ec.message().c_str());
    }
  });
}

void IRAM_ATTR Session::init(const session::Inject &di) { // static
  if (units.empty()) { // headunit creation/destruction aligned with desk session
    create_units();
  }

  active::session.emplace(di).handshake();
}

bool Session::send_ctrl_msg(const JsonDocument &doc) {
  std::array<char, 128> packed;
  auto msg_bytes = serializeMsgPack(doc, packed.data(), packed.size());
  ESP_LOGD(TAG.data(), "sending ctrl msg, bytes=%d", msg_bytes);

  msg_len = htons(msg_bytes);
  auto buff_seq = std::array{asio::buffer(&msg_len, MSG_LEN_SIZE), //
                             asio::buffer(packed.data(), msg_bytes)};

  // NOTE: send the handshake synchronously
  error_code ec;
  auto tx_bytes = socket_ctrl.send(buff_seq, 0, ec);

  if (ec || (tx_bytes != (msg_bytes + MSG_LEN_SIZE))) {
    ESP_LOGW(TAG.data(), "send_ctrl_msg failed, bytes=%u/%u reason: %s", tx_bytes, msg_bytes,
             ec.message().c_str());

    return false;
  }

  return true;
}

void IRAM_ATTR Session::shutdown() {
  [[maybe_unused]] error_code ec;
  idle_timer.cancel(ec);

  if (socket_data.is_open()) {
    ESP_LOGI(TAG.data(), "shutting down data_handle=%d", socket_data.native_handle());

    socket_data.cancel(ec);
    socket_data.shutdown(udp_socket::shutdown_both, ec);
    socket_data.close(ec);
  }

  if (socket_ctrl.is_open()) {
    ESP_LOGI(TAG.data(), "shutting down ctrl_handle=%d", socket_ctrl.native_handle());

    socket_ctrl.cancel(ec);
    socket_ctrl.shutdown(tcp_socket::shutdown_both, ec);
    socket_ctrl.close(ec);
  }

  if (dmx) {
    dmx->stop(); // sockets are closed, safe to stop DMX
    dmx.reset();
  }

  // execute the final clean up (reset of active session optional) outside the
  // scope of this function
  asio::defer(server_io_ctx, [] { active::session.reset(); });
}

/*
constructor notes:
  1. socket is passed as a reference to a reference so we must move to our local
socket reference

asyncLoop() notes:
  1. nothing within this function can be captured by the lamba
     because the scope of this function ends before
     the lamba executes

  2. the async_* call will attach the lamba to the io_ctx
     then immediately return and then this function returns

  3. we capture a shared_ptr (self) for access within the lamba,
     that shared_ptr is kept in scope while async_read is
     waiting for data on the socket then during execution
     of the lamba

  4. when called again from within the lamba the sequence of
     events repeats (this function returns) and the shared_ptr
     once again goes out of scope

  5. the crucial point -- we must keep the use count
     of the session above zero until the session ends
     (e.g. due to error, natural completion, io_ctx is stopped)

within the lamba:

  1. since this isn't captured all access to member functions/variables
     must use self

  2. in general, we try to minimize logic in the lamba and instead call
     member functions to get back within this context

  3. check the socket status frequently and bail out if error
     (bailing out implies allowing the shared_ptr to go out of scope)

  4. upon receipt of the packet length we check if that many bytes are
     available (for debug) then async_* them in

  5. if at any point the socket isn't ready fall through and release the
     shared_ptr

  6. otherwise schedule async_* of next packet

  7. reminder this session will auto destruct if more async_ work isn't
scheduled

misc notes:
  1. the first return of this function traverses back to the Server that created
the Session (in the same io_ctx)
  2. subsequent returns are to the io_ctx and match the required void return
signature
*/

} // namespace desk
} // namespace ruth
