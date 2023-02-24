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
#include "dmx/dmx.hpp"
#include "dmx/frame.hpp"
#include "headunit/ac_power.hpp"
#include "headunit/dimmable.hpp"
#include "ru_base/time.hpp"

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

static void self_destruct(void *) noexcept {
  ESP_LOGD(Session::TAG, "self-destruct");

  shared::active_session.reset();
}

// Object API

Session::Session(tcp_socket &&sock) noexcept
    : ctrl_sock(std::move(sock)),              // move the accepted socket
      data_sock(ctrl_sock.get_executor()),     // data sock (connected later)
      idle_shutdown(10000),                    // default, may be overriden
      stats_interval(2000),                    // default, may be overriden
      ctrl_packed(MsgIn::default_packed_size), // ctrl msg stream_buff
      data_packed(MsgIn::default_packed_size), // data msg stream_buff
      ctrl_packed_out{0x00},                   // write packed ctrl msg buffer
      data_packed_out{0x00},                   // write packed data msg buffer
      stats_timer{nullptr},                    // esp_timer to calc stats via separate task
      destruct_timer{nullptr}                  // esp_timer to destruct via separate task
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
  if (ec) ESP_LOGD(TAG, "close(): error=%s", ec.message().c_str());

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
                          esp_timer_start_periodic(stats_timer, stats_interval.count() * 1000);
                          return;
                        }

                        close(ec);
                      });
}

void IRAM_ATTR Session::ctrl_msg_process(MsgIn &&msg) noexcept {
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

  auto msg = MsgIn(ctrl_packed);

  if (msg.calc_packed_len() == true) {
    ESP_LOGI(TAG, "ctrl msg waiting in stream buffer");
    ctrl_msg_process(std::move(msg));
    return;
  }

  desk::async_read(ctrl_sock, std::move(msg), [this](const error_code ec, MsgIn &&msg) mutable {
    if (!ec) ctrl_msg_process(std::move(msg));
  });
}

void IRAM_ATTR Session::data_msg_read() noexcept {

  auto msg = MsgIn(data_packed);

  if (msg.calc_packed_len() == true) {
    // we already have a message in the buffer
    ESP_LOGI(TAG, "data msg waiting in stream buffer");
    data_msg_reply(std::move(msg), Elapsed());
    return;
  }

  desk::async_read(data_sock, std::move(msg),
                   [this, msg_wait = Elapsed()](const error_code ec, MsgIn &&msg) mutable {
                     msg_wait.freeze();

                     data_msg_reply(ec, std::move(msg), std::move(msg_wait));
                   });
}

void IRAM_ATTR Session::data_msg_reply(MsgIn &&msg_in, const Elapsed &&msg_wait) noexcept {
  StaticDoc doc_in;

  if (!msg_in.deserialize_into(doc_in) || !msg_in.can_render()) {
    close(io::make_error(errc::protocol_error));
    return;
  }

  stats->saw_frame();
  dmx->tx_frame(msg_in.dframe<dmx::frame>());

  for (auto &unit : units) {
    unit->handle_msg(doc_in);
  }

  StaticDoc doc_out;
  auto msg_out = MsgOut(desk::FEEDBACK, doc_out, data_packed_out);
  msg_out.take_elapsed(std::move(msg_in.e));

  msg_out.add_kv(desk::SEQ_NUM, doc_in[desk::SEQ_NUM].as<uint32_t>());
  msg_out.add_kv(desk::DATA_WAIT_US, msg_wait);
  msg_out.add_kv(desk::ECHO_NOW_US, doc_in[desk::NOW_US]);
  msg_out.add_kv(desk::FPS, stats->cached_fps());

  // dmx stats
  msg_out.add_kv(desk::DMX_QOK, dmx->q_ok());
  msg_out.add_kv(desk::DMX_QRF, dmx->q_rf());
  msg_out.add_kv(desk::DMX_QSF, dmx->q_sf());

  msg_out.add_kv(desk::ELAPSED_US, msg_out.e.freeze());
  msg_out.serialize();

  asio::async_write(ctrl_sock, msg_out.write_buff(), msg_out.write_bytes(),
                    [this](const error_code ec, std::size_t bytes) {
                      if (!ec && bytes) {
                        data_msg_read(); // wait for next data msg
                        idle_watch_dog();

                      } else {
                        close(ec);
                      }
                    });
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
      ctrl_sock, msg_out.write_buff(), msg_out.write_bytes(),
      [this](const error_code ec, std::size_t bytes) mutable {
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
