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
#include "io/async_msg2.hpp"
#include "io/io.hpp"
#include "io/msg_static.hpp"
#include "ru_base/time.hpp"
#include "ru_base/uint8v.hpp"
#include "session/session.hpp"

namespace ruth {
namespace desk {

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

void IRAM_ATTR Session::data_feedback(const JsonDocument &data_doc, const int64_t async_us,
                                      Elapsed &elapsed) {

  DRAM_ATTR static io::StaticPacked packed;

  auto msg = io::Msg(io::FEEDBACK, packed);

  msg.add_kv(io::SEQ_NUM, data_doc[io::SEQ_NUM].as<uint32_t>());
  msg.add_kv(io::NOW_US, rut::raw_us());
  msg.add_kv(io::ASYNC_US, async_us);
  msg.add_kv(io::ELAPSED_US, elapsed().count());
  msg.add_kv(io::ECHOED_NOW_US, data_doc[io::NOW_US]);
  msg.add_kv(io::JITTER_US, async_us - data_doc[io::SYNC_WAIT_US].as<int64_t>());
  msg.add_kv(io::FPS, stats.cached_fps());

  auto ec = io::write_msg(socket_ctrl, msg);

  log_feedback(ec);
}

void IRAM_ATTR Session::data_msg_rx() {
  DRAM_ATTR static io::StaticPacked packed;

  io::async_read_msg( //
      *socket_data,   //
      packed,         //
      [this, async_start_us = rut::raw_us()](const error_code ec, io::Msg msg) {
        const auto async_us = rut::raw_us() - async_start_us;
        JsonDocument &doc = msg.doc;

        if (success(ec)) {                                          // no socket error, confirm doc
          if (!doc.isNull() && (doc[io::MAGIC] == io::MAGIC_VAL)) { // doc is good
            Elapsed elapsed;

            stats.saw_frame();
            idle_watch_dog(); // reset the idle watchdog, we received a data msg

            dmx->tx_frame(msg.dframe<dmx::frame>());

            for (auto &unit : units) {
              unit->handleMsg(doc);
            }

            data_feedback(doc, async_us, elapsed);
          } else { // doc bad
            ESP_LOGW(TAG.data(), "not renderable, is_null=%s magic=0x%x",
                     doc.isNull() ? "true " : "false", doc[io::MAGIC].as<uint16_t>());
          }

          // prepare for next msg (no socket error)
          data_msg_rx();
        }
      });
}

void IRAM_ATTR Session::connect_data(Port port) {
  auto address = socket_ctrl.remote_endpoint().address();
  auto endpoint = tcp_endpoint(address, port);
  socket_data.emplace(server_io_ctx);

  asio::async_connect(*socket_data, std::array{endpoint},
                      [this](const error_code ec, const tcp_endpoint remote_endpoint) {
                        if (!ec) {
                          socket_data->set_option(ip_tcp::no_delay(true));

                          ESP_LOGI(TAG.data(), "data socket connected=%s:%d handle=%d",
                                   remote_endpoint.address().to_string().c_str(),
                                   remote_endpoint.port(), socket_data->native_handle());

                          fps_calc();
                          data_msg_rx();

                        } else {
                          ESP_LOGW(TAG.data(), "data socket failed, reason=%s",
                                   ec.message().c_str());
                        }
                      });
}

void IRAM_ATTR Session::fps_calc() {
  stats_timer.expires_after(stats_interval);
  stats_timer.async_wait([this](const error_code ec) {
    if (!ec) {
      stats.calc();

      fps_calc();
    }
  });
}

// sends initial handshake
void IRAM_ATTR Session::handshake_part1() {
  DRAM_ATTR static io::StaticPacked packed;
  io::Msg msg(packed);

  msg.add_kv(io::TYPE, io::HANDSHAKE);
  msg.add_kv(io::NOW_US, rut::now_epoch<Micros>().count());
  msg.add_kv(io::REF_US, local_ref_time.count());

  if (auto ec = io::write_msg(socket_ctrl, msg); !ec) {
    handshake_part2();
  }
}

// receives final handshake
void IRAM_ATTR Session::handshake_part2() {
  DRAM_ATTR static io::StaticPacked packed;

  io::async_read_msg( //
      socket_ctrl,    //
      packed,         //
      [this](const error_code ec, io::Msg msg) {
        JsonDocument &doc = msg.doc;

        if (!ec && !doc.isNull() && (io::HANDSHAKE == csv(doc[io::TYPE])) &&
            (doc[io::DATA_PORT])) {
          // proper reply to handshake
          Port port = doc[io::DATA_PORT];
          int64_t idle_ms = doc[io::IDLE_SHUTDOWN_US] | idle_shutdown.count();
          idle_shutdown = Millis(idle_ms);
          remote_ref_time = Micros(doc[io::REF_US] | 0);

          if (port) {           // we got a port
            dmx = DMX::init();  // spin up DMX
            connect_data(port); // connect to the data port
          }
        } else {
          ESP_LOGW(TAG.data(), "failed, reason=%s", ec.message().c_str());
          shutdown();
        }
      });
}

void IRAM_ATTR Session::idle_watch_dog() {
  auto expires = rut::as_duration<Seconds, Millis>(idle_shutdown);
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

  active::session.emplace(di);
}

void IRAM_ATTR Session::shutdown() {
  [[maybe_unused]] error_code ec;
  idle_timer.cancel(ec);
  stats_timer.cancel(ec);

  if (socket_ctrl.is_open()) {
    ESP_LOGI(TAG.data(), "shutting down ctrl handle=%d", socket_ctrl.native_handle());
    socket_ctrl.close(ec);
  }

  if (socket_data.has_value() && socket_data->is_open()) {
    ESP_LOGI(TAG.data(), "shutting down data handle=%d", socket_data->native_handle());
    socket_data->close(ec);
    socket_data.reset();
  }

  if (dmx) {
    dmx->stop(); // sockets are closed, safe to stop DMX
    dmx.reset();
  }

  // execute the final clean up (reset of active session optional) outside the
  // scope of this function
  asio::post(server_io_ctx, [] { active::session.reset(); });
}

} // namespace desk
} // namespace ruth
