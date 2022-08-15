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
static void async_loop(shSession, shDMX dmx);
static void create_units();
static void idle_watch_dog(shSession session, shDMX dmx);
static void shutdown(shSession session, shDMX dmx = shDMX());

IRAM_ATTR void async_loop(shSession session, shDMX dmx) {
  session->msg_len = 0;

  udp_socket &socket = session->data_socket;

  auto start_us = esp_timer_get_time();
  socket.async_receive( //
      asio::buffer(session->packed), [=](error_code ec, size_t bytes) {
        if (ec) {
          ESP_LOGW(TAG.data(), "recv msg failed, bytes= %d reason=%s", bytes,
                   ec.message().c_str());
          shutdown(session, dmx);
          return;
        }

        const auto async_us = esp_timer_get_time() - start_us;

        if ((async_us < 2500) || (async_us > 30000)) {
          ESP_LOGI(TAG.data(), "async_us=%lld", async_us);
        }
        // now that we have the entire packed message
        // attempt to create the DeskMsg, ask DMX to send the frame then
        // ask each headunit to handle it's part of the message
        if (auto msg = DeskMsg(session->packed, bytes); msg.validMagic()) {
          dmx->txFrame(msg.dframe<DMX::Frame>());

          for (auto unit : units) {
            unit->handleMsg(msg.root());
          }
        }

        idle_watch_dog(session, dmx); // reset the idle watchdog

        // call ourself and keep shared_ptrs in scope
        async_loop(session, dmx);
      });

  /*

    // we reuse ec so no const
    asio::async_read(session->socket, asio::buffer(&msg_len, MSG_LEN_SIZE),
                     asio::transfer_exactly(MSG_LEN_SIZE), [=](error_code ec, size_t bytes) {
                       msg_len = ntohs(msg_len); // network order to host order

                       if (!ec) {
                         bytes = asio::read(session->socket, asio::buffer(packed),
                                            asio::transfer_exactly(msg_len), ec);
                         // confirm the read was successful and we got the bytes we need
                         if (!ec && (bytes == msg_len)) {
                           idle_watch_dog(session, dmx);

                           // now that we have the entire packed message
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
                         } else {
                           ESP_LOGW(TAG.data(), "read msg failed, wanted=%d got=%d reason=%s",
                                    msg_len, bytes, ec.message().c_str());
                         }

                       } else {
                         ESP_LOGW(TAG.data(), "async_read() failed reason=%s",
    ec.message().c_str()); shutdown(session, dmx);
                       }
                     });
    */
}

static void create_units() {
  units.emplace_back(std::make_shared<AcPower>("ac power"));
  units.emplace_back(std::make_shared<DiscoBall>("disco ball", 1)); // pwm 1
  units.emplace_back(std::make_shared<ElWire>("el dance", 2));      // pwm 2
  units.emplace_back(std::make_shared<ElWire>("el entry", 3));      // pwm 3
  units.emplace_back(std::make_shared<LedForest>("led forest", 4)); // pwm 4
}

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
    session->data_socket.cancel(ec);
    session->idle_timer.cancel(ec);

    if (dmx) {
      dmx->stop();
    }
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

  // creates a new session shared_ptr, saves as active session
  auto session = active_session.emplace(new Session(di));

  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();

  uint16_t data_port = session->data_socket.local_endpoint().port();
  root["data_port"] = data_port;

  static std::array<char, 128> packed;
  static uint16_t msg_len = 0;
  auto bytes = serializeMsgPack(doc, packed.data(), packed.size());

  msg_len = htons(bytes);
  auto buff_seq =
      std::array{asio::buffer(&msg_len, MSG_LEN_SIZE), asio::buffer(packed.data(), bytes)};

  ESP_LOGI(TAG.data(), "sending setup msg, bytes=%d", bytes);

  asio::async_write(
      session->socket, buff_seq, [=](const error_code ec, [[maybe_unused]] size_t bytes) {
        if (ec) {
          ESP_LOGI(TAG.data(), "async_write() failed, reason=%s", ec.message().c_str());

          shutdown(session);
          return;
        }

        ESP_LOGI(TAG.data(), "udp data_port=%d", data_port);
      });

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
