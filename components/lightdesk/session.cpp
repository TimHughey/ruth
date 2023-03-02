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
#include "async/msg_in.hpp"
#include "async/msg_out.hpp"
#include "async/read.hpp"
#include "async/write.hpp"
#include "dmx/dmx.hpp"
#include "dmx/frame.hpp"
#include "headunit/ac_power.hpp"
#include "headunit/dimmable.hpp"
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
    : ctrl_sock(std::move(sock)),          // move the accepted socket
      data_sock(ctrl_sock.get_executor()), // data sock (connected later)
      idle_shutdown(10000),                // default, may be overriden
      stats_interval(2000),                // default, may be overriden
      ctrl_packed(1024),                   // ctrl msg stream_buff
      data_packed(1024),                   // data msg stream_buff
      ctrl_packed_out{0x00},               // write packed ctrl msg buffer
      data_packed_out{0x00},               // write packed data msg buffer
      stats_timer{nullptr},                // esp_timer to calc stats via separate task
      destruct_timer{nullptr}              // esp_timer to destruct via separate task
{
  // headunits are static outside of class, make sure they are created
  if (units.empty()) create_units();

  // create the idle timeout (self-destruct) timer
  esp_timer_create_args_t args{self_destruct, nullptr, ESP_TIMER_TASK, "desk::session", true};
  esp_timer_create(&args, &destruct_timer);

  // reuse timer args, changing necessary values
  args.callback = fps_calc;
  args.arg = this;
  args.name = "desk::session.stats";

  esp_timer_create(&args, &stats_timer);

  dmx = std::make_unique<DMX>();

  handshake();
}

Session::~Session() noexcept {
  // stop and free all esp_timers
  const auto timers = std::array{stats_timer, destruct_timer};

  std::for_each(timers.begin(), timers.end(), [](auto *timer) {
    if (!timer) return;
    esp_timer_stop(timer);
    esp_timer_delete(std::exchange(timer, nullptr));
  });

  // graceful shutdown
  [[maybe_unused]] error_code ec;
  data_sock.close(ec);
  ctrl_sock.close(ec);

  std::for_each(units.begin(), units.end(), [](auto &unit) { unit->dark(); });

  // stop dmx and wait for confirmation
  if (dmx && (dmx->stop().get() == true)) dmx.reset();
}

void Session::close(const error_code ec) noexcept {
  if (ec) ESP_LOGI(TAG, "close(): error=%s", ec.message().c_str());

  if (!destruct_timer) {
    idle_shutdown = Millis(0);
    idle_watch_dog();
    return; // allow timer to handle destruct
  }

  // fallen through, self-destruct is already in-progress
  ESP_LOGI(TAG, "close(): self destruct in-progress");
}

void IRAM_ATTR Session::connect_data(Port port) noexcept {
  auto address = ctrl_sock.remote_endpoint().address();
  auto endpoint = tcp_endpoint(address, port);

  asio::async_connect(data_sock, std::array{endpoint},
                      [this](const error_code ec, const tcp_endpoint r) {
                        if (!ec) {
                          data_sock.set_option(ip_tcp::no_delay(true));

                          data_msg_read();
                          // esp_timer requires duration in microseconds
                          esp_timer_start_periodic(stats_timer, stats_interval.count() * 1000);

                        } else {
                          close(ec);
                        }
                      });
}

void IRAM_ATTR Session::ctrl_msg_process(MsgIn msg) noexcept {
  StaticDoc doc;

  idle_watch_dog();

  if (!msg.deserialize_into(doc)) {
    close(io::make_error(errc::protocol_error));
    return;
  };

  csv type{doc[desk::TYPE].as<const char *>()}; // get msg type

  if (csv{desk::HANDSHAKE} == type) { // the handshake reply
    idle_shutdown = Millis(doc[desk::IDLE_SHUTDOWN_MS] | idle_shutdown.count());
    Port port = doc[desk::DATA_PORT] | 0;

    if (port) connect_data(port);

    // start stats reporting
    stats.emplace(Millis(doc[desk::STATS_MS] | stats_interval.count()));
  } else if (csv{desk::SHUTDOWN} == type) {
    close();
    return;
  }

  ctrl_msg_read();
}

void IRAM_ATTR Session::ctrl_msg_read() noexcept {
  // note: we do not reset the idle watch dog for ctrl messages
  //       idle is based entirely on data messages
  desk::async::read_msg(ctrl_sock, MsgIn(ctrl_packed), [this](MsgIn &&msg) mutable {
    if (msg.xfer_ok()) {
      ctrl_msg_process(std::move(msg));
    } else {
      close(msg.ec);
    }
  });
}

void IRAM_ATTR Session::data_msg_read() noexcept {

  // buffer empty or incomplete, must read from socket
  desk::async::read_msg(data_sock, MsgIn(data_packed), [this](MsgIn msg_in) mutable {
    if (msg_in.xfer_ok()) {
      data_msg_reply(std::move(msg_in));
    } else {
      close(msg_in.ec);
    }
  });
}

void IRAM_ATTR Session::data_msg_reply(MsgIn msg_in) noexcept {
  // first capture the wait time to receive the data msg
  const auto msg_in_wait = msg_in.elapsed();

  // note: create MsgOut as early as possible to capture elapsed duration
  StaticDoc doc_out;
  auto msg_out = MsgOut(desk::FEEDBACK, doc_out, data_packed_out);

  StaticDoc doc_in;

  if (!msg_in.deserialize_into(doc_in) || !msg_in.can_render(doc_in)) {
    close(io::make_error(errc::protocol_error));
    return;
  }

  stats->saw_frame();
  dmx->tx_frame(msg_in.dframe<dmx::frame>(doc_in));

  for (auto &unit : units) {
    unit->handle_msg(doc_in);
  }

  msg_out.copy_kv(doc_in, doc_out, desk::SEQ_NUM);

  msg_out.add_kv(desk::DATA_WAIT_US, msg_in_wait);
  msg_out.add_kv(desk::ECHO_NOW_US, doc_in[desk::NOW_US]);
  msg_out.add_kv(desk::FPS, stats->cached_fps());

  // dmx stats
  msg_out.add_kv(desk::DMX_QOK, dmx->q_ok());
  msg_out.add_kv(desk::DMX_QRF, dmx->q_rf());
  msg_out.add_kv(desk::DMX_QSF, dmx->q_sf());

  msg_out.add_kv(desk::ELAPSED_US, msg_out.elapsed());

  async::write_msg(ctrl_sock, std::move(msg_out), [this](MsgOut msg_out) {
    if (msg_out.xfer_ok()) {
      data_msg_read(); // wait for next data msg
      idle_watch_dog();

    } else {
      close(msg_out.ec);
    }
  });
}

void IRAM_ATTR Session::fps_calc(void *self) noexcept { // static
  static_cast<desk::Session *>(self)->stats->calc();
}

// sends initial handshake
void IRAM_ATTR Session::handshake() noexcept {
  StaticDoc doc_out;

  idle_watch_dog();

  auto msg_out = MsgOut(desk::HANDSHAKE, doc_out, ctrl_packed_out);

  msg_out.add_kv(desk::NOW_US, rut::now_epoch<Micros>().count());
  msg_out.serialize();

  // HANDSHAKE PART ONE:  write a minimal data feedback message to the ctrl sock
  asio::async_write( //
      ctrl_sock, msg_out.write_buff(), [this](const error_code ec, std::size_t bytes) mutable {
        if (!ec && bytes) {
          // handshake message sent, move to ctrl msg loop
          ctrl_msg_read();
          return;
        }

        ESP_LOGW(TAG, "handshake failed: %s", ec.message().c_str());
        close(ec);
      });
}

void IRAM_ATTR Session::idle_watch_dog() noexcept {

  if (ctrl_sock.is_open()) {
    esp_timer_stop(destruct_timer);
    esp_timer_start_once(destruct_timer, idle_shutdown.count() * 1000);
  }
}

} // namespace desk
} // namespace ruth
