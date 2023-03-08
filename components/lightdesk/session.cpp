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
#include "async/read.hpp"
#include "async/write.hpp"
#include "dmx/dmx.hpp"
#include "dmx/frame.hpp"
#include "headunit/ac_power.hpp"
#include "headunit/dimmable.hpp"
#include "msg/out.hpp"
#include "ru_base/rut.hpp"

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

/// @brief Self-destruct a Session via esp_timer
/// @param  pointer unused
static void self_destruct(void *) noexcept {
  ESP_LOGD(Session::TAG, "self-destruct");

  shared::active_session.reset();
}

// Object API

Session::Session(tcp_socket &&sock) noexcept
    : ctrl_sock(std::move(sock)),                            // move the accepted socket
      data_sock(ctrl_sock.get_executor()),                   // data sock (connected later)
      idle_shutdown(10000),                                  // default, may be overriden
      stats_interval(2000),                                  // default, may be overriden
      stats_timer(ctrl_sock.get_executor(), stats_interval), // periodic stats reporting
      destruct_timer{nullptr} // esp_timer to destruct via separate task
{
  // headunits are static outside of class, make sure they are created
  if (units.empty()) create_units();

  // create the idle timeout (self-destruct) timer
  esp_timer_create_args_t args{self_destruct, nullptr, ESP_TIMER_TASK, "desk::session", true};
  esp_timer_create(&args, &destruct_timer);

  dmx = std::make_unique<DMX>();

  handshake();
}

Session::~Session() noexcept {

  if (destruct_timer) {
    esp_timer_stop(destruct_timer);
    esp_timer_delete(std::exchange(destruct_timer, nullptr));
  }

  // graceful shutdown
  [[maybe_unused]] error_code ec;
  stats_timer.cancel(ec);
  data_sock.close(ec);
  ctrl_sock.close(ec);

  std::for_each(units.begin(), units.end(), [](auto &unit) { unit->dark(); });

  // stop dmx and wait for confirmation
  if (dmx && (dmx->stop().get() == true)) dmx.reset();
}

void Session::close(const error_code ec) noexcept {
  if (!destruct_timer) {
    ESP_LOGI(TAG, "close() error=%s", ec.message().c_str());
    idle_shutdown = Millis(0);
    idle_watch_dog();
    return; // allow timer to handle destruct
  }

  // fallen through, self-destruct is already in-progress
}

void IRAM_ATTR Session::connect_data(Port port) noexcept {
  auto address = ctrl_sock.remote_endpoint().address();
  auto endpoint = tcp_endpoint(address, port);

  asio::async_connect(
      data_sock, std::array{endpoint}, [this](const error_code ec, const tcp_endpoint r) {
        if (!ec) {
          ESP_LOGI(TAG, "socket=%d data connection established", data_sock.native_handle());

          data_sock.set_option(ip_tcp::no_delay(true));

          asio::post(ctrl_sock.get_executor(), [this]() {
            read_data_msg(MsgIn());
            report_stats();
          });

        } else {
          close(ec);
        }
      });
}

void IRAM_ATTR Session::ctrl_msg_process(MsgIn &&msg) noexcept {
  if (msg.xfer_ok()) {

    DynamicJsonDocument doc(Msg::default_doc_size);

    idle_watch_dog();

    if (!msg.deserialize_into(doc)) {
      close(io::make_error(errc::protocol_error));
      return;
    };

    // we have what we need from the message, schedule the next read
    read_ctrl_msg(std::move(msg));

    csv type{doc[desk::MSG_TYPE].as<const char *>()}; // get msg type

    if (csv{desk::HANDSHAKE} == type) { // the handshake reply
      idle_shutdown = Millis(doc[desk::IDLE_SHUTDOWN_MS] | idle_shutdown.count());
      Port port = doc[desk::DATA_PORT] | 0;

      ESP_LOGI(TAG, "socket=%d received handshake, data_port=%u", ctrl_sock.native_handle(), port);

      if (port) connect_data(port);

      // start stats reporting
      stats.emplace(Millis(doc[desk::STATS_MS] | stats_interval.count()));
    } else if (csv{desk::SHUTDOWN} == type) {
      close();
      return;
    }
  } else {
    close(msg.ec);
  }
}

void IRAM_ATTR Session::data_msg_reply(MsgIn &&msg_in) noexcept {
  if (msg_in.xfer_ok()) {
    // first capture the wait time to receive the data msg
    const auto msg_in_wait = msg_in.elapsed();

    DynamicJsonDocument doc_in(Msg::default_doc_size);

    if (!msg_in.deserialize_into(doc_in) || !msg_in.can_render(doc_in)) {
      close(io::make_error(errc::illegal_byte_sequence));
      return;
    }

    stats->saw_frame();
    dmx->tx_frame(MsgIn::dframe<dmx::frame>(doc_in));

    for (auto &unit : units) {
      unit->handle_msg(doc_in);
    }

    // note: create MsgOut as early as possible to capture elapsed duration
    DynamicJsonDocument doc_out(Msg::default_doc_size);
    auto msg_out = MsgOut(desk::FEEDBACK);
    msg_out.add_kv(desk::SEQ_NUM, doc_in[desk::SEQ_NUM].as<uint32_t>());
    msg_out.add_kv(desk::DATA_WAIT_US, msg_in_wait);
    msg_out.add_kv(desk::ECHO_NOW_US, doc_in[desk::NOW_US].as<int64_t>());
    msg_out.add_kv(desk::ELAPSED_US, msg_out.elapsed());

    async::write_msg(ctrl_sock, std::move(msg_out), [this](MsgOut msg_out) {
      if (msg_out.xfer_ok()) {
        idle_watch_dog();

      } else {
        close(msg_out.ec);
      }
    });

    // we've consumed what we needed from the message, reuse it for next read
    read_data_msg(std::move(msg_in));

  } else {
    // message in failed
    close(msg_in.ec);
  }
}

// sends initial handshake
void IRAM_ATTR Session::handshake() noexcept {

  idle_watch_dog();

  auto msg_out = MsgOut(desk::HANDSHAKE);

  msg_out.add_kv(desk::NOW_US, rut::raw_us());
  // HANDSHAKE PART ONE:  write a minimal to the ctrl sock

  async::write_msg(ctrl_sock, std::move(msg_out), [this](MsgOut msg) {
    if (msg.xfer_ok()) {
      read_ctrl_msg(MsgIn());
    } else {
      ESP_LOGW(TAG, "handshake failed: %s", msg.ec.message().c_str());
      close(msg.ec);
    }
  });
}

void IRAM_ATTR Session::idle_watch_dog() noexcept {

  if (ctrl_sock.is_open()) {
    esp_timer_stop(destruct_timer);
    esp_timer_start_once(destruct_timer, idle_shutdown.count() * 1000);
  }
}

void IRAM_ATTR Session::read_ctrl_msg(MsgIn &&msg) noexcept {

  if (!ctrl_sock.is_open()) return;

  // note: we forward the message since it may contain data from the previous read
  async::read_msg(ctrl_sock, std::move(msg), [this](MsgIn &&msg) {
    // ESP_LOGI(TAG, "ctrl_msg_read n=%u", n);

    ctrl_msg_process(std::forward<MsgIn>(msg));
  });
}

void IRAM_ATTR Session::read_data_msg(MsgIn &&msg) noexcept {

  if (!data_sock.is_open()) return;

  // note: we forward the message since it may contain data from the previous read
  async::read_msg(data_sock, std::move(msg),
                  [this](MsgIn &&msg) { data_msg_reply(std::forward<MsgIn>(msg)); });
}

void IRAM_ATTR Session::report_stats() noexcept {

  stats_timer.expires_after(stats_interval);
  stats_timer.async_wait([this](error_code ec) {
    if (ec) return;

    stats->calc();

    MsgOut msg(desk::STATS);

    msg.add_kv(desk::FPS, stats->cached_fps());
    msg.add_kv(desk::DMX_QOK, dmx->q_ok());
    msg.add_kv(desk::DMX_QRF, dmx->q_rf());
    msg.add_kv(desk::DMX_QSF, dmx->q_sf());

    async::write_msg(ctrl_sock, std::move(msg), [this](MsgOut msg) {
      if (msg.xfer_ok()) {
        report_stats();
      }
    });
  });
}

} // namespace desk
} // namespace ruth
