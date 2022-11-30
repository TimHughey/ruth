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

#include "ArduinoJson.h"
#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <chrono>
#include <cmath>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <memory>
#include <mutex>
#include <vector>

#include "dmx/dmx.hpp"
#include "dmx/frame.hpp"
#include "headunit/ac_power.hpp"
#include "headunit/discoball.hpp"
#include "headunit/elwire.hpp"
#include "headunit/headunit.hpp"
#include "headunit/ledforest.hpp"
#include "inject/inject.hpp"
#include "io/async_msg.hpp"
#include "io/io.hpp"
#include "io/msg_static.hpp"
#include "ru_base/time.hpp"
#include "ru_base/uint8v.hpp"
#include "session/session.hpp"

namespace ruth {
namespace desk {

using HeadUnits = std::vector<shHeadUnit>;
DRAM_ATTR static HeadUnits units;

static void create_units() {
  units.emplace_back(std::make_unique<AcPower>("ac power"));
  units.emplace_back(std::make_unique<DiscoBall>("disco ball", 1)); // pwm 1
  units.emplace_back(std::make_unique<ElWire>("el dance", 2));      // pwm 2
  units.emplace_back(std::make_unique<ElWire>("el entry", 3));      // pwm 3
  units.emplace_back(std::make_unique<LedForest>("led forest", 4)); // pwm 4
}

// Object API

void IRAM_ATTR Session::ctrl_msg_loop() noexcept {
  DRAM_ATTR static io::StaticPacked packed;

  idle_watch_dog();

  io::async_read_msg( //
      ctrl_sock,      //
      packed,         //
      [s = shared_from_this()](const error_code ec, io::Msg msg) {
        JsonDocument &doc = msg.doc;
        csv type{doc[io::TYPE].as<const char *>()}; // get msg type

        if (ec || doc.isNull()) { // fall out of scope
          ESP_LOGW(TAG.data(), "ctrl_msg_loop: %s", ec.message().c_str());
          return;
        }

        if (csv{io::HANDSHAKE} == type) { // the handshake reply

          s->idle_shutdown = Millis(doc[io::IDLE_SHUTDOWN_MS] | s->idle_shutdown.count());
          s->remote_ref_time = Micros(doc[io::REF_US] | 0);
          Port port = doc[io::DATA_PORT] | 0;

          if (port) {              // we got a port
            s->connect_data(port); // connect to the data port
          } else {
            ESP_LOGE(TAG.data(), "data_port=0");
          }
        }
      });
}

void IRAM_ATTR Session::data_msg_loop() noexcept {
  DRAM_ATTR static io::StaticPacked packed;

  idle_watch_dog();

  io::async_read_msg( //
      *data_sock,     //
      packed,         //
      [s = shared_from_this(), msg_wait = Elapsed()](const error_code ec, io::Msg msg) mutable {
        Elapsed e;

        msg_wait.freeze();
        JsonDocument &doc = msg.doc;
        const uint16_t magic = doc[io::MAGIC] | 0x0000;

        if (ec || (magic != io::MAGIC_VAL)) {
          ESP_LOGE(TAG.data(), "magic=%04x %s", magic, ec.message().c_str());
          return; // fall out of scope
        }

        s->stats.saw_frame();
        s->idle_watch_dog(); // reset the idle watchdog, we received a data msg

        s->dmx->tx_frame(msg.dframe<dmx::frame>());

        for (auto &unit : units) {
          unit->handleMsg(doc);
        }

        DRAM_ATTR static io::StaticPacked packed;
        auto tx_msg = io::Msg(io::FEEDBACK, packed);

        tx_msg.add_kv(io::SEQ_NUM, doc[io::SEQ_NUM].as<uint32_t>());
        tx_msg.add_kv(io::DATA_WAIT_US, msg_wait);
        tx_msg.add_kv(io::ELAPSED_US, e.freeze());
        tx_msg.add_kv(io::ECHO_NOW_US, doc[io::NOW_US]);
        tx_msg.add_kv(io::FPS, s->stats.cached_fps());

        // dmx stats
        tx_msg.add_kv(io::DMX_QOK, s->dmx->q_ok());
        tx_msg.add_kv(io::DMX_QRF, s->dmx->q_rf());
        tx_msg.add_kv(io::DMX_QSF, s->dmx->q_sf());

        s->idle_watch_dog();

        auto &ctrl_sock = s->ctrl_sock;

        io::async_write_msg( //
            ctrl_sock, std::move(tx_msg), [s = std::move(s)](const error_code ec) mutable {
              if (ec) { // write failed
                ESP_LOGW(TAG.data(), "data_msg_rx: %s", ec.message().c_str());
                return; // fall out of scope, idle timeout will clean up
              }

              s->data_msg_loop(); // wait for next data msg
            });
      });
}

void IRAM_ATTR Session::connect_data(Port port) noexcept {
  auto address = ctrl_sock.remote_endpoint().address();
  auto endpoint = tcp_endpoint(address, port);
  data_sock.emplace(server_io_ctx);

  asio::async_connect(*data_sock, std::array{endpoint},
                      [s = shared_from_this()](const error_code ec, const tcp_endpoint r) {
                        if (ec) {
                          ESP_LOGW(TAG.data(), "connect_data: %s", ec.message().c_str());
                          return; // fall out of scope, idle timeout will clean up
                        }

                        auto &data_sock = s->data_sock; // get ref to avoid pointer dereference

                        data_sock->set_option(ip_tcp::no_delay(true));

                        const auto &l = data_sock->local_endpoint();

                        ESP_LOGI(TAG.data(), "%s:%d -> %s:%d data connected, handle=%d",
                                 l.address().to_string().c_str(), l.port(),
                                 r.address().to_string().c_str(), r.port(),
                                 data_sock->native_handle());

                        s->fps_calc();
                        s->data_msg_loop();
                      });
}

void IRAM_ATTR Session::fps_calc() noexcept {
  stats_timer.expires_after(stats_interval);
  stats_timer.async_wait([s = shared_from_this()](const error_code ec) {
    if (ec) return; // timer shutdown

    s->stats.calc();
    s->fps_calc();
  });
}

// sends initial handshake
void IRAM_ATTR Session::handshake(std::shared_ptr<Session> session) noexcept { // static
  DRAM_ATTR static io::StaticPacked packed;

  session->idle_watch_dog();

  io::Msg msg(io::HANDSHAKE, packed);

  msg.add_kv(io::NOW_US, rut::now_epoch<Micros>().count());

  // HANDSHAKE PART ONE:  write a minimal data feedback message to the ctrl sock
  auto &ctrl_sock = session->ctrl_sock;
  io::async_write_msg( //
      ctrl_sock, std::move(msg), [s = session->shared_from_this()](const error_code ec) mutable {
        if (ec) { // write failed
          ESP_LOGW(TAG.data(), "handshake: %s", ec.message().c_str());

          return; // fall out of scope, idle timeout will detect
        }

        // handshake message sent, move to ctrl msg loop
        s->ctrl_msg_loop();
      });
}

void IRAM_ATTR Session::idle_watch_dog() noexcept {
  static auto expires = rut::as_duration<Seconds, Millis>(idle_shutdown);

  if (ctrl_sock.is_open()) {
    idle_timer.expires_after(expires);
    idle_timer.async_wait([s = shared_from_this()](const error_code ec) {
      // if the timer ever expires then we're idle
      if (ec) {
        ESP_LOGD(TAG.data(), "idle_watch_dog: %s", ec.message().c_str());
        return;
      }

      ESP_LOGI(TAG.data(), "idle timeout");

      { // graceful shutdown
        [[maybe_unused]] error_code ec;

        if (s->data_sock.has_value()) {
          s->data_sock->shutdown(tcp_socket::shutdown_both, ec);
          s->data_sock->close(ec);
        }

        s->ctrl_sock.close(ec);
        s->ctrl_sock.shutdown(tcp_socket::shutdown_both, ec);

        s->idle_timer.cancel(ec);
        s->stats_timer.cancel(ec);

        std::for_each(units.begin(), units.end(), [](auto &unit) { unit->dark(); });

        if (s->dmx && (s->dmx->stop().get() == true)) { // stop dmx and wait for confirmation
          s->dmx.reset();
        }
      }
    });
  }
}

std::shared_ptr<Session> IRAM_ATTR Session::init(const session::Inject &di) { // static
  if (units.empty()) { // headunit creation/destruction aligned with desk session
    create_units();
  }

  auto session = std::shared_ptr<Session>(new Session(di));
  session->dmx = DMX::init(); // spin up DMX

  handshake(session->shared_from_this());
  return session->shared_from_this();
}

} // namespace desk
} // namespace ruth
