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
#include "ru_base/rut.hpp"

#include "ArduinoJson.h"
#include <algorithm>
#include <array>
#include <asio.hpp>
#include <chrono>
#include <cmath>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <functional>
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
static void self_destruct(void *self_v) noexcept {

  ESP_LOGI(Session::TAG, "self-destruct, session=%p", self_v);

  shared::active_session.reset();
}

// static class data to ensure a single task is running
// at any given time
DRAM_ATTR TaskHandle_t Session::th{nullptr};

// Object API

Session::Session(io_context &io_ctx, tcp_socket &&sock) noexcept
    : io_ctx(io_ctx),             // creator owns our io_context
      data_sock(std::move(sock)), // all socket comms
      idle_ms(10000),             // default, may be overriden
      stats_interval(2000),       // default, may be overriden
      stats_timer{nullptr},       // periodic stats reporting
      destruct_timer{nullptr}     // esp_timer to destruct via separate task

{
  data_sock.set_option(ip_tcp::no_delay(true));

  // headunits are static outside of class, make sure they are created
  if (units.empty()) create_units();

  // create the idle timeout (self-destruct) timer
  esp_timer_create_args_t args{self_destruct, this, ESP_TIMER_TASK, "desk::session", true};
  esp_timer_create(&args, &destruct_timer);

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

  // start the main message loop

  ESP_LOGI(TAG, "startup complete, task_rc=%d", rc);
}

Session::~Session() noexcept {

  const auto timers = std::array{std::ref(destruct_timer), std::ref(stats_timer)};
  std::for_each(timers.begin(), timers.end(), [](auto t) {
    esp_timer_stop(t);
    esp_timer_delete(std::exchange(t.get(), nullptr));
  });

  // graceful shutdown
  [[maybe_unused]] error_code ec;
  data_sock.close(ec);

  std::for_each(units.begin(), units.end(), [](auto &unit) { unit->dark(); });

  // stop dmx and wait for confirmation
  dmx.reset();
}

void Session::close(const error_code ec) noexcept {
  if (!destruct_timer) {
    ESP_LOGI(TAG, "close() error=%s", ec.message().c_str());
    idle_ms = Millis(0);
    idle_watch_dog();
    return; // allow timer to handle destruct
  }

  // fallen through, self-destruct is already in-progress
}

void IRAM_ATTR Session::msg_loop(MsgIn &&msg_in) noexcept {

  if (data_sock.is_open() == false) return; // prevent tight error loops

  // note: we move the message since it may contain data from the previous read
  async_msg::read(data_sock, std::move(msg_in), [this](MsgIn &&msg_in) {
    // intentionally little code in this lambda

    idle_watch_dog();
    msg_process(std::forward<MsgIn>(msg_in));
  });
}

void IRAM_ATTR Session::msg_process(MsgIn &&msg_in) noexcept {
  // first capture the wait time to receive the data msg
  const auto msg_in_wait = msg_in.elapsed();
  Elapsed e;

  // bail out on error
  if (msg_in.xfer_error()) {
    close(msg_in.ec);
    return;
  }

  // create the doc for msg_in. all data will be copied to the
  // JsonDocument so msg_in is not required beyond this point
  DynamicJsonDocument doc_in(Msg::default_doc_size);

  if (!msg_in.deserialize_into(doc_in)) {
    close(io::make_error(errc::illegal_byte_sequence));
    return;
  };

  // msg_in is not used after deserialization so we can immediately
  // prepare for the next incoming message.  note:  this is an async
  // function and returns immediately
  msg_loop(std::move(msg_in));

  if (MsgIn::is_msg_type(doc_in, desk::DATA) && MsgIn::valid(doc_in)) {
    // note: create MsgOut as early as possible to capture elapsed duration
    MsgOut msg_out(desk::DATA_REPLY);

    if (stats) stats->saw_frame();

    if (dmx) dmx->tx_frame(doc_in[desk::FRAME].as<JsonArrayConst>());

    std::for_each(units.begin(), units.end(), [&doc_in](auto &u) { u->handle_msg(doc_in); });

    msg_out.add_kv(desk::SEQ_NUM, doc_in[desk::SEQ_NUM].as<uint32_t>());
    msg_out.add_kv(desk::DATA_WAIT_US, msg_in_wait);
    msg_out.add_kv(desk::ECHO_NOW_US, doc_in[desk::NOW_US].as<int64_t>());
    msg_out.add_kv(desk::ELAPSED_US, msg_out.elapsed());

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
    idle_ms = Millis(doc_in[desk::IDLE_MS] | idle_ms.count());
    frame_len = doc_in[desk::FRAME_LEN] | 256;

    const auto lep = data_sock.local_endpoint();
    const auto rep = data_sock.remote_endpoint();

    ESP_LOGI(TAG, "received handshake, local=%u remote=%u, frame_len=%u", lep.port(), rep.port(),
             frame_len);

    dmx = std::make_unique<DMX>(frame_len);

    stats.emplace(Millis(doc_in[desk::STATS_MS] | stats_interval.count())); // start stats reporting
    esp_timer_start_periodic(stats_timer, stats_interval.count() * 1000);
    // end of handshake message handling

  } else if (Msg::is_msg_type(doc_in, desk::SHUTDOWN)) {
    close();
    // end of shutdown message handling

  } else {
    const auto type = MsgIn::type(doc_in);

    ESP_LOGI(TAG, "unhandled msg type=%s", type.c_str());
  }
}

void IRAM_ATTR Session::idle_watch_dog() noexcept {

  if (data_sock.is_open()) {
    esp_timer_stop(destruct_timer);
    esp_timer_start_once(destruct_timer, idle_ms.count() * 1000);
  }
}

void IRAM_ATTR Session::post_stats() noexcept {
  if (!stats_pending) {
    asio::defer(io_ctx, [this]() {
      stats_periodic.clear(); // ensure nothing from previous report

      stats_periodic.add(desk::SUPP, true);
      stats_periodic.add(desk::FPS, stats->cached_fps());

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

  if (self->data_sock.is_open() && stats.has_value() && dmx) {
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
  vTaskSuspend(th);
}

} // namespace desk
} // namespace ruth
