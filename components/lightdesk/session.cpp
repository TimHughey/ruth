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
#include "ru_base/uint8v.hpp"

#include "ArduinoJson.h"
#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <chrono>
#include <cmath>
#include <esp_log.h>
#include <mutex>
#include <vector>

namespace ruth {
namespace desk {

using namespace std::chrono_literals;
static constexpr csv TAG = "DeskSession";

typedef std::vector<shHeadUnit> HeadUnits;
DRAM_ATTR static HeadUnits units;

static std::optional<shSession> active_session;

// forward decl of static functions for Session
static void async_loop(shSession session, shDMX dmx);
static void create_units();
static void idle_watch_dog(shSession session, shDMX dmx);
static void shutdown(shSession session, shDMX dmx);

DRAM_ATTR DeskMsg::Packed packed;
DRAM_ATTR uint16_t msg_len;
static constexpr size_t MSG_LEN_SIZE = sizeof(msg_len);

IRAM_ATTR static void async_loop(shSession session, shDMX dmx) {

  asio::async_read(                         // async read the length of the packet
      session->socket,                      // read from socket
      asio::buffer(&msg_len, MSG_LEN_SIZE), // into this buffer
      asio::transfer_exactly(MSG_LEN_SIZE), // the size of the pending packet
      [session = session, dmx = dmx](const error_code ec, size_t rx_bytes) {
        idle_watch_dog(session, dmx); // reset the idle watchdog

        msg_len = ntohs(msg_len); // network order to host order

        if (!ec) {
          // read the remainder of the packet
          asio::async_read(session->socket,                 //
                           asio::buffer(packed),            //
                           asio::transfer_exactly(msg_len), //
                           [=](const error_code ec, [[maybe_unused]] size_t rx_bytes) {
                             if (ec) {
                               ESP_LOGW(TAG.data(), "async_read() part two failed reason=%s",
                                        ec.message().c_str());
                               shutdown(session, dmx);
                             }

                             // second, now that we have the entire packed message
                             // attempt to create the DeskMsg, ask DMX to send the frame then
                             // ask each headunit to handle it's part of the message
                             if (auto msg = DeskMsg(packed, msg_len); msg.validMagic()) {
                               dmx->txFrame(msg.dframe<DMX::Frame>());

                               for (auto unit : units) {
                                 unit->handleMsg(msg.root());
                               }
                             }

                             // call ourself and keep shared_ptrs in scope
                             async_loop(session, dmx);
                           });
        } else {
          ESP_LOGW(TAG.data(), "async_read() failed reason=%s", ec.message().c_str());
          shutdown(session, dmx);
        }
      });
}

static void create_units() {
  units.emplace_back(std::make_shared<AcPower>("ac power"));
  units.emplace_back(std::make_shared<DiscoBall>("disco ball", 1)); // pwm 1
  units.emplace_back(std::make_shared<ElWire>("el dance", 2));      // pwm 2
  units.emplace_back(std::make_shared<ElWire>("el entry", 3));      // pwm 3
  units.emplace_back(std::make_shared<LedForest>("led forest", 4)); // pwm 4
}

/*
IRAM_ATTR static const error_code handle_msg(tcp_socket &socket, shDMX dmx) {
  // std::array<char, 256> msg_buff;

  auto buff_seq = std::array{mut_buffer(msg)}

  error_code msg_ec;
  auto bytes = asio::read(                                    // read the rest of the packet
      socket,                                                 // from this socket
      asio::mutable_buffer(msg_buff.data(), msg_buff.size()), // put the bytes read here
      asio::transfer_exactly(msg_len),                        // read the exact bytes specified
      msg_ec);

  // confirm the read was a success and we received all the message bytes
  if (!msg_ec) {
    if (bytes == msg_len) { // only handle complete messages
      auto msg = DeskMsg(msg_buff, bytes);

      if (msg.validMagic()) {
        dmx->txFrame(msg.dframe<DMX::Frame>());

        for (auto unit : units) {
          unit->handleMsg(msg.root());
        }
      }
    } else {
      ESP_LOGW(TAG.data(), "handle_msg() got bytes=%d want_bytes=%d\n", bytes, msg_len);
    }
  }

  return msg_ec;
}
*/

IRAM_ATTR static void idle_watch_dog(shSession session, shDMX dmx) {
  auto expires = ru_time::as_duration<Seconds, Millis>(session->idle_shutdown);
  session->idle_timer.expires_after(expires);
  session->idle_timer.async_wait([=](const error_code ec) {
    // if the timer ever expires then we're idle
    if (!ec) {
      std::for_each(units.begin(), units.end(), [](shHeadUnit unit) { unit->dark(); });

      ESP_LOGI(TAG.data(), "is idle");

      shutdown(session, dmx);

    } else {
      ESP_LOGD(TAG.data(), "idleWatchDog() terminating reason=%s", ec.message().c_str());
    }
  });
}

IRAM_ATTR static void shutdown(shSession session, shDMX dmx) {
  if (active_session.has_value() && (active_session.value() == session)) {
    ESP_LOGD(TAG.data(), "shutting down session=%p", session.get());
    active_session.reset();
    ESP_LOGD(TAG.data(), "active_session=%s", active_session.has_value() ? "true" : "false");

    [[maybe_unused]] error_code ec;
    session->socket.cancel(ec);
    session->idle_timer.cancel(ec);

    dmx->stop();
  }
}

// Object API

IRAM_ATTR shSession Session::activeSession() { // static
  return active_session.value_or(shSession());
}

void Session::start(const session::Inject &di) { // static
  if (units.empty()) { // headunit creation/destruction aligned with desk session
    create_units();
  }

  // creates a new session shared_ptr, saves as active session then
  // schedules work via asyncLoop
  auto session = active_session.emplace(new Session(di));
  async_loop(session, DMX::start());
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
