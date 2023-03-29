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
#include "ArduinoJson.h"
#include "async_msg/read.hpp"
#include "desk_msg/in.hpp"
#include "dmx/dmx.hpp"
#include "headunit/ac_power.hpp"
#include "headunit/dimmable.hpp"
#include "network/network.hpp"
#include "ru_base/clock_now.hpp"
#include "stats/stats.hpp"

#include <algorithm>
#include <array>
#include <asio.hpp>
#include <cmath>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/timers.h>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace ruth {

namespace desk {

// headunits
DRAM_ATTR static std::vector<std::unique_ptr<HeadUnit>> units;

static void create_units() noexcept {
  if (units.empty()) {
    units.emplace_back(std::make_unique<AcPower>("ac power"));
    units.emplace_back(std::make_unique<Dimmable>("disco ball", 1)); // pwm 1
    units.emplace_back(std::make_unique<Dimmable>("el dance", 2));   // pwm 2
    units.emplace_back(std::make_unique<Dimmable>("el entry", 3));   // pwm 3
    units.emplace_back(std::make_unique<Dimmable>("led forest", 4)); // pwm 4
  }
}

static auto create_timer_args(auto callback, Session *session, const char *name) noexcept {
  return esp_timer_create_args_t{callback, session, ESP_TIMER_TASK, name, true /* skip missed */};
}

// class level tracking of at most two sessions
DRAM_ATTR std::array<std::unique_ptr<Session>, 2> Session::sessions;

// Object API
Session::Session(io_context &io_ctx, tcp_socket &&peer) noexcept
    : io_ctx(io_ctx),             // creator owns our io_context
      sess_sock(std::move(peer)), // read/write session control
      data_sock(io_ctx)           // read only data socket (connected during handshake)
{
  sess_sock.set_option(ip_tcp::no_delay(true));

  // create the idle timeout timer
  auto timer_args = create_timer_args(&idle_timeout, this, "desk::idle_timeout");
  esp_timer_create(&timer_args, &idle_timer);

  auto rc =                    // create the task using a static desk_stack
      xTaskCreate(&run_io_ctx, // static func to start task
                  TAG,         // task name
                  10'240,      // desk_stack size
                  this,        // none, use shared::desk
                  7,           // priority
                  &th);        // task handle

  ESP_LOGI(TAG, "startup complete, task_rc=%d", rc);
}

Session::~Session() noexcept {

  // stop the timers immediately
  const auto timers = std::array{std::ref(idle_timer), std::ref(stats_timer)};
  std::for_each(timers.begin(), timers.end(), [](auto t) {
    if (t) {
      esp_timer_stop(t);
      esp_timer_delete(std::exchange(t.get(), nullptr));
    }
  });

  // graceful shutdown
  using sock_ref = std::reference_wrapper<tcp_socket>;
  std::array<sock_ref, 2> socks{std::ref(sess_sock), std::ref(data_sock)};
  error_code ec;

  for (tcp_socket &s : socks) {
    if (s.is_open()) {
      s.shutdown(tcp_socket::shutdown_both, ec);
      s.close(ec);
    }
  }

  std::for_each(units.begin(), units.end(), [](auto &unit) { unit->dark(); });

  // stop dmx (blocks until shutdown is complete)
  dmx.reset();

  if (th) {
    vTaskSuspend(th);
    vTaskDelete(std::exchange(th, nullptr));
  }

  ESP_LOGI(TAG, "session=%p freed", this);
}

void Session::close(const error_code ec) noexcept {
  if (!io_ctx.stopped()) {

    io_ctx.stop();

    // self-destruct is not
    ESP_LOGI(TAG, "close() error=%s", ec.message().c_str());
  }
}

void Session::create(io_context &io_ctx, tcp_socket &&peer) noexcept {
  // note: create() is always called from a different task so it can perform
  //       actions on a Session task (e.g. suspend, delete) directly

  // headunits are static outside of class, make sure they are created
  create_units();

  // ensure only a single session is active
  if (sessions.front().get()) {
    // there is an active session, end it
    sessions.front().reset();
  }

  sessions.front() = std::unique_ptr<Session>(new Session(io_ctx, std::forward<tcp_socket>(peer)));
}

void IRAM_ATTR Session::data_msg_loop(MsgIn &&msg_in_data) noexcept {

  if (data_sock.is_open() == false) return;

  // note: we move the message since it may contain data from the previous read
  async_msg::read(data_sock, std::move(msg_in_data), [this](MsgIn &&msg_in) {
    // first capture the wait time to receive the data msg
    if (msg_in.xfer_ok()) dmx->track_data_wait(msg_in.elapsed());

    data_msg_process(std::move(msg_in));
  });
}

void IRAM_ATTR Session::data_msg_process(MsgIn &&msg_in_data) noexcept {

  // create the doc for msg_in. all data will be copied to the
  // JsonDocument so msg_in is not required beyond this point
  StaticJsonDocument<740> doc_in;

  // when msg is good and deserialized, send stats, then wait for next message
  if (msg_in_data.deserialize_into(doc_in)) {

    if (MsgIn::is_msg_type(doc_in, desk::DATA) && MsgIn::valid(doc_in)) {
      JsonArrayConst fdata_array = doc_in[desk::FRAME].as<JsonArrayConst>();

      std::array<uint8_t, 25> fdata{0x00};
      auto it = fdata.begin();
      for (JsonVariantConst b : fdata_array) {
        *it = b;
        std::advance(it, 1);
      }

      dmx->next_frame(std::move(fdata), std::distance(fdata.begin(), it));

      std::for_each(units.begin(), units.end(), [&doc_in](auto &u) { u->handle_msg(doc_in); });

      StaticJsonDocument<384> doc_out;
      doc_out[desk::MSG_TYPE] = desk::STATS;

      if (dmx->stats_pending()) dmx->stats_populate(doc_out);

      doc_out[desk::ECHO_NOW_US] = doc_in[desk::NOW_US].as<int64_t>();
      doc_out[desk::MAGIC] = desk::MAGIC_VAL;

      static std::array<uint8_t, 256> storage;

      const auto packed_len = serializeMsgPack(doc_out, storage.data(), storage.size());

      asio::async_write(sess_sock, asio::buffer(storage.data(), packed_len),
                        [=, this, msg_reuse = std::move(msg_in_data)](const error_code &ec,
                                                                      std::size_t n) mutable {
                          if (ec || (n != packed_len)) {
                            close(ec);
                          } else {
                            // all is well, doc deserialized
                            idle_watch_dog(); // restart idle watch
                            data_msg_loop(std::move(msg_reuse));
                          }
                        });
    }
  } else {
    close(io::make_error(errc::illegal_byte_sequence));
    // do not start next message read
  }
}

// note: idle_watch_dog does not check for initial connnection
//       timeout because the socket is already connected by
//       lightdesk before creating the session
void IRAM_ATTR Session::idle_watch_dog() noexcept {

  if (idle_timer && !io_ctx.stopped()) {
    if (sess_sock.is_open() || data_sock.is_open()) {
      if (esp_timer_is_active(idle_timer)) {
        esp_timer_restart(idle_timer, idle_us);
      } else {
        esp_timer_start_periodic(idle_timer, idle_us);
      }
    }
  }
}

void Session::idle_timeout(void *self_v) noexcept {
  auto *self = static_cast<Session *>(self_v);

  const auto matched{self == active().get() ? "true" : "false"};

  if (matched) {
    ESP_LOGI(Session::TAG, "idle timeout fired, match active session=%s", matched);
    self->close(io::make_error(errc::timed_out));
  }
}

void IRAM_ATTR Session::sess_msg_loop(MsgIn &&msg_in) noexcept {

  if (sess_sock.is_open()) { // prevent tight error loops

    idle_watch_dog(); // restart idle watch

    // note: we move the message since it may contain data from the previous read
    async_msg::read(sess_sock, std::move(msg_in), [this](MsgIn &&msg_in) {
      if (msg_in.xfer_ok()) {
        sess_msg_process(std::forward<MsgIn>(msg_in));
      } else {
        close(msg_in.ec);
      }
    });
  }
}

void IRAM_ATTR Session::sess_msg_process(MsgIn &&msg_in) noexcept {

  // create the doc for msg_in. all data will be copied to the
  // JsonDocument so msg_in is not required beyond this point
  StaticJsonDocument<740> doc_in;

  if (!msg_in.deserialize_into(doc_in)) {
    close(io::make_error(errc::illegal_byte_sequence));
    return;
  };

  if (Msg::is_msg_type(doc_in, desk::HANDSHAKE)) {

    // set idle microseconds if specified in msg
    const int64_t idle_ms_raw = doc_in[desk::IDLE_MS] | 0;
    if (!idle_ms_raw) idle_us = idle_ms_raw * 1000;

    frame_len = doc_in[desk::FRAME_LEN] | 256;

    // stats starts on creation
    const uint32_t stats_ms = doc_in[desk::STATS_MS] | 2000;
    dmx = std::make_unique<DMX>(Stats(stats_ms));

    // create and start stats timer
    auto timer_args = create_timer_args(&report_stats, this, "desk::report_stats");
    esp_timer_create(&timer_args, &stats_timer);
    esp_timer_start_periodic(stats_timer, stats_ms * 1000);

    // open the data socket
    const auto rip = sess_sock.remote_endpoint().address();
    const int16_t rport = doc_in[desk::DATA_PORT].as<uint16_t>();
    data_sock.async_connect(tcp_endpoint(rip, rport), [this](const error_code &ec) {
      if (!ec) {
        data_sock.set_option(ip_tcp::no_delay(true));
        data_msg_loop(MsgIn());
      }
    });

    ESP_LOGI(TAG, "handshake, frame_len=%u dmx=%p data_port=%u", frame_len, dmx.get(), rport);
    // end of handshake message handling

  } else if (Msg::is_msg_type(doc_in, desk::SHUTDOWN)) {
    close();
    // end of shutdown message handling

  } else {
    ESP_LOGI(TAG, "unhandled msg type=%s", MsgIn::type(doc_in).c_str());
  }

  // done with msg_in, queue receive of next msg
  if (io_ctx.stopped() == false) sess_msg_loop(std::move(msg_in));
}

void IRAM_ATTR Session::report_stats(void *self_v) noexcept {
  auto self = static_cast<Session *>(self_v);
  auto *dmx = self->dmx.get();

  if (!self->io_ctx.stopped() && dmx) dmx->stats_calculate();
}

void IRAM_ATTR Session::run_io_ctx(void *self_v) noexcept {
  auto self = static_cast<Session *>(self_v);

  // reset the io_ctx, we could be reusing it
  self->io_ctx.reset();

  // ensure io_ctx has work before starting it
  asio::post(self->io_ctx, [self]() { self->sess_msg_loop(MsgIn()); });

  self->io_ctx.run();

  ESP_LOGI(TAG, "io_ctx work completed, suspending task");
  auto timer = xTimerCreate("sess_end",        // name
                            pdMS_TO_TICKS(10), // expires after
                            pdTRUE,            // auto reload
                            self,              // pass ourself as a check
                            &self_destruct);   // callback

  xTimerStart(timer, pdMS_TO_TICKS(100));

  vTaskSuspend(self->th);
}

/// @brief Self-destruct a Session via esp_timer
/// @param  pointer unused
void Session::self_destruct(TimerHandle_t timer) noexcept {
  auto *self = static_cast<Session *>(pvTimerGetTimerID(timer));

  if (!sessions.front() || (self != sessions.front().get())) {
    ESP_LOGI(TAG, "attempt to self-destruct wrong session");
    xTimerDelete(std::exchange(timer, nullptr), pdMS_TO_TICKS(10));
    return;
  }

  TaskStatus_t info;
  vTaskGetInfo(self->th,  // task handle
               &info,     // where to store info
               pdTRUE,    // calculate task stack high water mark
               eInvalid); // include task status

  ESP_LOGI(Session::TAG, "self-destruct, session=%p timer=%p status=%u stack_hw=%lu", //
           self, timer, info.eCurrentState, info.usStackHighWaterMark);

  const auto state = info.eCurrentState;

  // io_ctx hasn't stopped, restart the timer
  if (state == eSuspended) {

    // delete the timer, we know it's a good value since this function was
    // called by FreeRTOS
    xTimerDelete(std::exchange(timer, nullptr), pdMS_TO_TICKS(10));

    if (sessions.front()) {
      sessions.front().reset();
      ESP_LOGI(TAG, "active session reset");
    }

  } else if (state | (eRunning | eBlocked | eReady)) {
    ESP_LOGI(TAG, "task=%p is not suspended(3) state=%u, will retry", self, state);

    xTimerReset(timer, pdMS_TO_TICKS(10));
  }
}

} // namespace desk
} // namespace ruth
