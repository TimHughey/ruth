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
#include "headunit/dimmable.hpp"
#include "io/async_msg.hpp"
#include "io/io.hpp"
#include "io/msg_static.hpp"
#include "ru_base/time.hpp"
#include "session/session.hpp"

namespace ruth {

namespace shared {
std::optional<desk::Session> active_session;
}

namespace desk {

DRAM_ATTR static std::vector<std::unique_ptr<HeadUnit>> units;

static void create_units() noexcept {
  units.emplace_back(std::make_unique<AcPower>("ac power"));
  units.emplace_back(std::make_unique<Dimmable>("disco ball", 1)); // pwm 1
  units.emplace_back(std::make_unique<Dimmable>("el dance", 2));   // pwm 2
  units.emplace_back(std::make_unique<Dimmable>("el entry", 3));   // pwm 3
  units.emplace_back(std::make_unique<Dimmable>("led forest", 4)); // pwm 4
}

static void self_destruct(void *) noexcept {
  ESP_LOGD(Session::TAG.data(), "self-destruct");

  shared::active_session.reset();
}

// Object API

Session::Session(tcp_socket &&sock) noexcept
    : ctrl_sock(std::move(sock)),            // move the accepted socket
      idle_shutdown(10000),                  // default, may be overriden
      idle_timer(ctrl_sock.get_executor()),  // idle timer, same executor as ctrl_socket
      stats_interval(2000),                  // default, may be overriden
      stats_timer(ctrl_sock.get_executor()), // fps/stats timer, same executor as ctrl_socket
      destruct_timer{nullptr}                // esp_timer to destructor via separate task
{
  // headunits are static outside of class, make sure they are created
  if (units.empty()) create_units();

  dmx = std::make_unique<DMX>();

  handshake();
}

Session::~Session() noexcept {
  // graceful shutdown
  [[maybe_unused]] error_code ec;
  idle_timer.cancel(ec);
  stats_timer.cancel(ec);

  if (data_sock.has_value()) data_sock->close(ec);
  ctrl_sock.close(ec);

  std::for_each(units.begin(), units.end(), [](auto &unit) { unit->dark(); });

  // stop dmx and wait for confirmation
  if (dmx && (dmx->stop().get() == true)) dmx.reset();

  if (destruct_timer) esp_timer_delete(destruct_timer);
}

void Session::close() noexcept {
  if (destruct_timer) return; // self destruct in progress

  idle_shutdown = Millis(0);
  idle_watch_dog();
}

void IRAM_ATTR Session::connect_data(Port port) noexcept {
  auto address = ctrl_sock.remote_endpoint().address();
  auto endpoint = tcp_endpoint(address, port);
  data_sock.emplace(ctrl_sock.get_executor());

  asio::async_connect(*data_sock, std::array{endpoint},
                      [this](const error_code ec, const tcp_endpoint r) {
                        if (ec) return;

                        data_sock->set_option(ip_tcp::no_delay(true));

                        // io::log_accept_socket(TAG, "data"sv, *data_sock, r);

                        fps_calc();
                        data_msg_read();
                      });
}

void IRAM_ATTR Session::ctrl_msg_process(io::Msg &&msg) noexcept {
  JsonDocument &doc = msg.doc;
  csv type{doc[io::TYPE].as<const char *>()}; // get msg type

  if (csv{io::HANDSHAKE} == type) { // the handshake reply
    idle_shutdown = Millis(doc[io::IDLE_SHUTDOWN_MS] | idle_shutdown.count());
    remote_ref_time = Micros(doc[io::REF_US] | 0);
    Port port = doc[io::DATA_PORT] | 0;

    if (port) connect_data(port);

    // start stats reporting
    stats.emplace(Millis(doc[io::STATS_MS] | stats_interval.count()));
  } else if (csv{io::SHUTDOWN} == type) {
    close();
    return;
  }

  ctrl_msg_read();
}

void IRAM_ATTR Session::ctrl_msg_read() noexcept {
  DRAM_ATTR static io::StaticPacked packed;

  // note: we do not reset the idle watch dog for ctrl messages
  //       idle is based entirely on data messages

  io::async_read_msg(ctrl_sock, packed, [this](const error_code ec, io::Msg msg) {
    if (!ec) ctrl_msg_process(std::move(msg));
  });
}

void IRAM_ATTR Session::data_msg_read() noexcept {
  DRAM_ATTR static io::StaticPacked packed;

  idle_watch_dog();

  io::async_read_msg( //
      *data_sock,     //
      packed, [this, msg_wait = Elapsed()](const error_code ec, io::Msg msg) mutable {
        if (!ec) {
          msg_wait.freeze();
          data_msg_reply(std::move(msg), std::move(msg_wait));
        } else {
          close();
        }
      });
}

void IRAM_ATTR Session::data_msg_reply(io::Msg &&msg, const Elapsed &&msg_wait) noexcept {
  Elapsed e;

  JsonDocument &doc = msg.doc;

  if (!msg.can_render()) return;

  stats->saw_frame();
  dmx->tx_frame(msg.dframe<dmx::frame>());

  for (auto &unit : units) {
    unit->handle_msg(doc);
  }

  DRAM_ATTR static io::StaticPacked packed;
  auto tx_msg = io::Msg(io::FEEDBACK, packed);

  tx_msg.add_kv(io::SEQ_NUM, doc[io::SEQ_NUM].as<uint32_t>());
  tx_msg.add_kv(io::DATA_WAIT_US, msg_wait);
  tx_msg.add_kv(io::ECHO_NOW_US, doc[io::NOW_US]);
  tx_msg.add_kv(io::FPS, stats->cached_fps());

  // dmx stats
  tx_msg.add_kv(io::DMX_QOK, dmx->q_ok());
  tx_msg.add_kv(io::DMX_QRF, dmx->q_rf());
  tx_msg.add_kv(io::DMX_QSF, dmx->q_sf());

  tx_msg.add_kv(io::ELAPSED_US, e.freeze());

  io::async_write_msg(ctrl_sock, std::move(tx_msg), [this](const error_code ec) {
    if (!ec) {
      idle_watch_dog(); // reset
      data_msg_read();  // wait for next data msg
    } else {
      close();
    }
  });
}

void IRAM_ATTR Session::fps_calc() noexcept {
  stats_timer.expires_after(stats_interval);
  stats_timer.async_wait([this](const error_code ec) {
    if (ec) return; // timer shutdown

    stats->calc();
    fps_calc();
  });
}

// sends initial handshake
void IRAM_ATTR Session::handshake() noexcept {
  DRAM_ATTR static io::StaticPacked packed;

  idle_watch_dog();

  io::Msg msg(io::HANDSHAKE, packed);

  msg.add_kv(io::NOW_US, rut::now_epoch<Micros>().count());

  // HANDSHAKE PART ONE:  write a minimal data feedback message to the ctrl sock
  io::async_write_msg( //
      ctrl_sock, std::move(msg), [this](const error_code ec) mutable {
        if (ec) { // write failed
          ESP_LOGW(TAG.data(), "handshake: %s", ec.message().c_str());

          return; // fall out of scope, idle timeout will detect
        }

        // handshake message sent, move to ctrl msg loop
        ctrl_msg_read();
      });
}

void IRAM_ATTR Session::idle_watch_dog() noexcept {
  static auto expires = rut::as_duration<Seconds, Millis>(idle_shutdown);

  if (ctrl_sock.is_open()) {
    idle_timer.expires_after(expires);
    idle_timer.async_wait([this](const error_code ec) {
      if (ec) return;

      // if the timer ever expires then we're idle
      ESP_LOGI(TAG.data(), "idle timeout");

      if (destruct_timer) return;

      esp_timer_create_args_t args{self_destruct, nullptr, ESP_TIMER_TASK, "session", true};

      esp_timer_create(&args, &destruct_timer);
      esp_timer_start_once(destruct_timer, 0);
    });
  }
}

} // namespace desk
} // namespace ruth
