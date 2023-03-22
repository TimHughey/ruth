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
#include "async_msg/read.hpp"
#include "async_msg/write.hpp"
#include "desk_msg/out.hpp"
#include "dmx/dmx.hpp"
#include "headunit/ac_power.hpp"
#include "headunit/dimmable.hpp"
#include "network/network.hpp"
#include "ru_base/clock_now.hpp"
#include "ru_base/rut.hpp"

#include "ArduinoJson.h"
#include <algorithm>
#include <array>
#include <asio.hpp>
#include <chrono>
#include <cmath>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/timers.h>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace ruth {

namespace shared {
std::unique_ptr<desk::Session> active_session;
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

// Object API
Session::Session(io_context &io_ctx, tcp_socket &&sock) noexcept
    : io_ctx(io_ctx),             // creator owns our io_context
      data_sock(std::move(sock)), // all socket comms
      idle_us{10000 * 1000},      // default, may be overriden
      stats_ms(2000)              // default, may be overriden
{
  data_sock.set_option(ip_tcp::no_delay(true));

  // headunits are static outside of class, make sure they are created
  if (units.empty()) create_units();

  // create the idle timeout timer
  esp_timer_create_args_t args{&idle_timeout,        // callback
                               this,                 // user data
                               ESP_TIMER_TASK,       // dispatch method
                               "desk::idle_timeout", // name
                               true};                // skip missed

  esp_timer_create(&args, &idle_timer);

  // reuse the args from idle_timeout timer to create stats timer
  args.callback = &report_stats;
  args.name = "desk::stats";
  esp_timer_create(&args, &stats_timer);

  auto rc =                    // create the task using a static desk_stack
      xTaskCreate(&run_io_ctx, // static func to start task
                  TAG,         // task name
                  10 * 1024,   // desk_stack size
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
  error_code ec;
  data_sock.shutdown(tcp_socket::shutdown_both, ec);
  data_sock.close(ec);
  if (ec) ESP_LOGI(TAG, "data sock close ec=%s", ec.message().c_str());

  std::for_each(units.begin(), units.end(), [](auto &unit) { unit->dark(); });

  units.clear();

  // stop dmx (blocks until shutdown is complete)
  dmx.reset();

  if (th) vTaskDelete(std::exchange(th, nullptr));

  ESP_LOGI(TAG, "session=%p freed", this);
}

void Session::close(const error_code ec) noexcept {
  if (!io_ctx.stopped()) {

    io_ctx.stop();

    // self-destruct is not
    ESP_LOGI(TAG, "close() error=%s", ec.message().c_str());
  }
}

// note: idle_watch_dog does not check for initial connnection
//       timeout because the socket is already connected by
//       lightdesk before creating the session
void IRAM_ATTR Session::idle_watch_dog() noexcept {

  if (idle_timer && !io_ctx.stopped()) {
    if (data_sock.is_open()) {
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

  const auto matched{self == shared::active_session.get() ? "true" : "false"};

  if (matched) {
    ESP_LOGI(Session::TAG, "idle timeout fired, match active session=%s", matched);
    shared::active_session->close(io::make_error(errc::timed_out));
  }
}

void IRAM_ATTR Session::msg_loop(MsgIn &&msg_in) noexcept {

  if (data_sock.is_open()) { // prevent tight error loops

    idle_watch_dog(); // restart idle watch

    // note: we move the message since it may contain data from the previous read
    async_msg::read(data_sock, std::move(msg_in), [this](MsgIn &&msg_in) {
      if (msg_in.xfer_ok()) {
        msg_process(std::forward<MsgIn>(msg_in));
      } else {
        close(msg_in.ec);
      }
    });
  }
}

void IRAM_ATTR Session::msg_process(MsgIn &&msg_in) noexcept {
  // first capture the wait time to receive the data msg
  const auto msg_in_elapsed_us = msg_in.elapsed();

  // create the doc for msg_in. all data will be copied to the
  // JsonDocument so msg_in is not required beyond this point
  DynamicJsonDocument doc_in(Msg::default_doc_size);

  if (!msg_in.deserialize_into(doc_in)) {
    close(io::make_error(errc::illegal_byte_sequence));
    return;
  };

  if (dmx && MsgIn::is_msg_type(doc_in, desk::DATA) && MsgIn::valid(doc_in)) {
    // note: create MsgOut as early as possible to capture elapsed duration
    MsgOut msg_out(desk::DATA_REPLY);

    if (stats) stats->saw_frame();

    dmx->tx_frame(doc_in[desk::FRAME].as<JsonArrayConst>());

    std::for_each(units.begin(), units.end(), [&doc_in](auto &u) { u->handle_msg(doc_in); });

    // msg_out.add_kv(desk::SEQ_NUM, doc_in[desk::SEQ_NUM].as<uint32_t>());
    msg_out.add_kv(desk::DATA_WAIT_US, msg_in_elapsed_us);
    msg_out.add_kv(desk::ECHO_NOW_US, doc_in[desk::NOW_US].as<int64_t>());

    // add supplemental metrics, if pending
    if (stats_pending) {
      msg_out(std::move(stats_periodic));
      stats_pending = false;
    }

    async_msg::write(data_sock, std::move(msg_out), [this](MsgOut msg_out) {
      if (msg_out.xfer_error()) close(msg_out.ec);
    });
    // end of data message handling

  } else if (Msg::is_msg_type(doc_in, desk::HANDSHAKE)) {

    // set idle microseconds if specified in msg
    const int64_t idle_ms_raw = doc_in[desk::IDLE_MS] | 0;
    if (!idle_ms_raw) idle_us = idle_ms_raw * 1000;

    frame_len = doc_in[desk::FRAME_LEN] | 256;

    dmx = std::make_unique<DMX>(frame_len);

    // stats starts on creation
    const int64_t interval = doc_in[desk::STATS_MS] | stats_ms;
    stats = std::make_unique<Stats>(interval);

    esp_timer_start_periodic(stats_timer, stats_ms * 1000);

    ESP_LOGI(TAG, "handshake, frame_len=%u dmx=%p", frame_len, dmx.get());
    // end of handshake message handling

  } else if (Msg::is_msg_type(doc_in, desk::SHUTDOWN)) {
    close();
    // end of shutdown message handling

  } else {
    const auto type = MsgIn::type(doc_in);

    ESP_LOGI(TAG, "unhandled msg type=%s", type.c_str());
  }

  // prepare for next inbound message
  msg_loop(std::move(msg_in));
}

void IRAM_ATTR Session::post_stats() noexcept {
  if (dmx && !stats_pending) {
    asio::defer(io_ctx, [this]() {
      stats_periodic.clear(); // ensure nothing from previous report

      stats_periodic.add(desk::SUPP, true);
      stats_periodic.add(desk::FPS, stats->cached_fps());
      stats_periodic.add(desk::NOW_REAL_US, clock_now::real::us());

      // ask DMX to add it's stats
      dmx->populate_stats(stats_periodic);

      stats_pending = true;
    });
  } else {
    ESP_LOGW(TAG, "stats pending collision");
  }
}

void IRAM_ATTR Session::report_stats(void *self_v) noexcept {
  auto self = static_cast<Session *>(self_v);
  auto &stats = self->stats;
  auto &dmx = self->dmx;

  stats->calc();

  if (!self->io_ctx.stopped() && stats && dmx) {
    self->post_stats();
  }
}

void IRAM_ATTR Session::run_io_ctx(void *self_v) noexcept {
  auto self = static_cast<Session *>(self_v);

  // reset the io_ctx, we could be reusing it
  self->io_ctx.reset();

  // ensure io_ctx has work before starting it
  asio::post(self->io_ctx, [self]() { self->msg_loop(MsgIn()); });

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

  TaskStatus_t info;

  vTaskGetInfo(self->th,  // task handle
               &info,     // where to store info
               pdTRUE,    // calculate task stack high water mark
               eInvalid); // include task status

  ESP_LOGI(Session::TAG, "self-destruct, session=%p timer=%p status=%u stack_hw=%lu",
           shared::active_session.get(), timer, info.eCurrentState, info.usStackHighWaterMark);

  const auto state = info.eCurrentState;

  // io_ctx hasn't stopped, restart the timer
  if (state == eSuspended) {

    // delete the timer, we know it's a good value since this function was
    // called by FreeRTOS
    xTimerDelete(std::exchange(timer, nullptr), pdMS_TO_TICKS(10));

    // delete the task and check it is the task for the active session
    const auto to_delete = std::exchange(self->th, nullptr);
    vTaskDelete(to_delete);

    if (shared::active_session.get() == self) {
      ESP_LOGI(TAG, "reseting active_session...");
      shared::active_session.reset();
    } else {
      ESP_LOGI(TAG, "task to delete is not active session");
    }

  } else if (state | (eRunning | eBlocked | eReady)) {

    xTimerReset(timer, pdMS_TO_TICKS(10));
  }
}

} // namespace desk
} // namespace ruth
