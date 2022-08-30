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
#include "msg.hpp"
#include "ru_base/time.hpp"
#include "ru_base/uint8v.hpp"
#include "session/session.hpp"

namespace ruth {
namespace desk {

using namespace std::chrono_literals;

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
  static io::StaticDoc doc;
  static io::Packed buff;

  io::async_tld(    //
      *socket_data, //
      buff,         //
      doc,          //
      [this, async_start_us = rut::raw_us()](error_code ec, size_t bytes) {
        // if no error, handle the incoming UDP data msg
        const auto async_us = rut::raw_us() - async_start_us;

        if (!ec && bytes) {
          Elapsed elapsed;

          stats.saw_frame();
          idle_watch_dog(); // reset the idle watchdog, we received a data msg

          // now that we have the entire packed message
          // attempt to create the DeskMsg, ask DMX to send the frame then
          // ask each headunit to handle it's part of the message
          if (auto msg = DeskMsg(doc); msg.can_render()) {
            dmx->tx_frame(msg.dframe<dmx::frame>());

            for (auto &unit : units) {
              unit->handleMsg(msg.root());
            }
          } else {
            ESP_LOGW(TAG.data(), "not renderable, bad magic");
          }

          send_feedback(doc, async_us, elapsed);
          data_msg_receive();
        } else if (ec) {

          ESP_LOGW(TAG.data(), "recv msg failed, bytes= %d reason=%s", bytes,
                   ec.message().c_str());
          shutdown();
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
                          ESP_LOGI(TAG.data(), "data socket connected=%s:%d handle=%d",
                                   remote_endpoint.address().to_string().c_str(),
                                   remote_endpoint.port(), socket_data->native_handle());

                          fps_calc();
                          data_msg_receive();

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

void IRAM_ATTR Session::handshake() {
  auto doc = std::make_unique<io::StaticDoc>();
  auto root = doc->to<JsonObject>();

  root["type"] = "handshake";
  root["now_µs"] = rut::now_epoch<Micros>().count();
  root["ref_µs"] = local_ref_time.count();

  send_ctrl_msg(*doc); // send initial handshake request

  auto buff = std::make_unique<io::Packed>();
  auto &bref = *buff;
  auto &dref = *doc;

  // read the handshake reply
  io::async_tld(   //
      socket_ctrl, //
      bref,        //
      dref,        //
      [this, doc = std::move(doc), buff = std::move(buff)](error_code ec, size_t bytes) mutable {
        // if no error, handle the incoming UDP data msg
        if (!ec && bytes) {
          auto root = doc->as<JsonObject>();
          csv type = root["type"] | "unknown";
          Port port = root["data_port"] | 0;
          int64_t idle_ms = root["idle_shutdown_ms"] | idle_shutdown.count();
          idle_shutdown = Millis(idle_ms);
          remote_ref_time = Micros(root["ref_µs"] | 0);

          if (type == csv{"handshake"} && port) {
            dmx = DMX::init();
            connect_data(port);
          }
        } else {
          ESP_LOGW(TAG.data(), "failed, bytes=%u reason=%s", bytes, ec.message().c_str());
          shutdown();
        }

        doc.reset();
        buff.reset();
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

bool IRAM_ATTR Session::send_ctrl_msg(const JsonDocument &doc, bool async) {
  auto buff = std::make_unique<io::Packed>();

  auto mp_buff = buff->data() + MSG_LEN_SIZE;
  const auto mp_max = buff->size() - MSG_LEN_SIZE;
  uint16_t mp_bytes = serializeMsgPack(doc, mp_buff, mp_max);
  const uint16_t msg_len = htons(mp_bytes);

  memcpy(buff->data(), &msg_len, MSG_LEN_SIZE);
  const size_t to_tx = mp_bytes + MSG_LEN_SIZE;

  if (async) {
    auto b = asio::buffer(buff->data(), to_tx);
    socket_ctrl.async_write_some(
        b, [this, _b = std::move(buff), to_tx](const error_code ec, size_t tx_bytes) mutable {
          log_send_msg(ec, to_tx, tx_bytes);
          _b.reset();
        });
  } else {
    error_code ec;
    auto tx_bytes = socket_ctrl.send(asio::buffer(buff->data(), to_tx), 0, ec);

    return log_send_msg(ec, to_tx, tx_bytes);
  }

  return true; // async always returns true
}

bool IRAM_ATTR Session::send_feedback(const JsonDocument &data_doc, const int64_t async_us,
                                      Elapsed &elapsed) {
  DynamicJsonDocument doc(1024);
  JsonObject root = doc.to<JsonObject>();

  root["type"] = "feedback";
  root["seq_num"] = data_doc["seq_num"];
  root["now_µs"] = rut::raw_us();
  root["async_µs"] = async_us;
  root["elapsed_µs"] = elapsed().count();
  root["echoed_now_µs"] = data_doc["now_µs"];
  root["fps"] = stats.cached_fps();

  return send_ctrl_msg(doc, SEND_ASYNC);
}

void IRAM_ATTR Session::shutdown() {
  [[maybe_unused]] error_code ec;
  idle_timer.cancel(ec);
  stats_timer.cancel(ec);

  if (socket_ctrl.is_open()) {
    ESP_LOGD(TAG.data(), "shutting down ctrl handle=%d", socket_ctrl.native_handle());

    socket_ctrl.cancel(ec);
    socket_ctrl.shutdown(tcp_socket::shutdown_both, ec);
    socket_ctrl.close(ec);
  }

  if (socket_data->is_open()) {
    ESP_LOGD(TAG.data(), "shutting down data handle==%d", socket_ctrl.native_handle());

    socket_data->cancel(ec);
    socket_data->shutdown(tcp_socket::shutdown_both, ec);
    socket_data.reset();
  }

  if (dmx) {
    dmx->stop(); // sockets are closed, safe to stop DMX
    dmx.reset();
  }

  // execute the final clean up (reset of active session optional) outside the
  // scope of this function
  asio::defer(server_io_ctx, [] { active::session.reset(); });
}

} // namespace desk
} // namespace ruth
