/*
     protocols/dmx.cpp - Ruth Dmx Protocol Engine
     Copyright (C) 2020  Tim Hughey

     This program is free software: you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation, either version 3 of the License, or
     (at your option) any later version.

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with this program.  If not, see <http://www.gnu.org/licenses/>.

     https://www.wisslanding.com
 */

#include "dmx/dmx.hpp"
#include "base/ru_time.hpp"
#include "io/io.hpp"

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <iterator>
#include <soc/uart_periph.h>
#include <soc/uart_reg.h>

using namespace std::literals::chrono_literals;

namespace ruth {

namespace dmx {
pthread_t thread;
void join() { pthread_join(thread, nullptr); }
} // namespace dmx

// DMX frames per second
using FPS = std::chrono::duration<float, std::ratio<1, 44>>;

static constexpr uint_fast32_t FRAME_BREAK = 22; // num bits at
                                                 // 250,000 baud (8µs)
static constexpr Micros FRAME_MAB = 12us;
static constexpr Micros FRAME_BYTE = 44us;
static constexpr Micros FRAME_SC = FRAME_BYTE;
static constexpr Micros FRAME_MTBF = 44us;
static constexpr Micros FRAME_DATA = Micros(FRAME_BYTE * 512);
static constexpr Millis FRAME_STATS = Millis(2s);

// frame interval does not include the BREAK as it is handled by the UART
static constexpr Micros FRAME_US = FRAME_MAB + FRAME_SC + FRAME_DATA + FRAME_MTBF;
static constexpr Millis FRAME_MS = ru_time::as_duration<Micros, Millis>(FRAME_US);

// UART0 is the console, UART2 has a defect
static constexpr int uart_num = 1;

constexpr gpio_num_t TX_PIN = GPIO_NUM_17;
static esp_err_t uart_rc = ESP_FAIL;

DMX::DMX() : fps_timer(io_ctx) {
  // uart driver not installed or failed last time
  if (uart_rc == ESP_FAIL) {
    if (uart_rc = uart_driver_install(uart_num, 129, dmx::TX_LEN, 0, NULL, 0); uart_rc == ESP_OK) {
      uart_rc = uartInit();
    }
  }
}

IRAM_ATTR void DMX::fpsCalculate() {
  static uint64_t calcs = 0;
  static TimePoint fps_start = steady_clock::now();

  if (state == desk::SHUTDOWN) {
    return;
  }

  fps_timer.expires_at(fps_start + Millis(FRAME_STATS * calcs++));

  fps_timer.async_wait([this](const error_code ec) {
    if (ec != asio::error::basic_errors::operation_aborted) {
      ESP_LOGD(TAG.data(), "terminating resason=%s\n", ec.message().c_str());
    }

    if (stats.mark && stats.frame_count) {
      stats.fps = (stats.frame_count - stats.mark) / FRAME_STATS.count();

      ESP_LOGD(TAG.data(), "fps=%2.2f", stats.fps);
    }

    // save the frame count as of this invocation
    stats.mark = stats.frame_count;

    fpsCalculate(); // restart timer
  });
}

IRAM_ATTR void *DMX::run([[maybe_unused]] void *data) { // static
  ptr()->fpsCalculate();                                // adds work to io_context
  ptr()->io_ctx.run();

  return nullptr; // returns when all work is complete
}

void DMX::start() { // static
  pthread_attr_t attr;

  if (auto res = pthread_attr_init(&attr); res == 0) {
    pthread_attr_setstacksize(&attr, 4096);

    if (pthread_create(&dmx::thread, &attr, DMX::run, NULL) == 0) {
      ESP_LOGI(TAG.data(), "created thread=%d\n", dmx::thread);
      return;
    }
  }

  ESP_LOGE(TAG.data(), "failed to create pthread\n");
}

IRAM_ATTR void DMX::txFrame(dmx::Frame &&frame) {
  // wait up to the max time to transmit a TX frame
  static TickType_t frame_ticks = pdMS_TO_TICKS(FRAME_MS.count());

  frame.preventFlicker();

  asio::post(io_ctx, [self = ptr(), frame = std::move(frame)]() mutable {
    // always ensure the previous tx has completed which includes
    // the BREAK (low for 88us)
    if (uart_wait_tx_done(uart_num, frame_ticks) == ESP_OK) {
      // at the end of the TX the UART pulls the TX low to generate the BREAK
      // once the code reaches this point the BREAK is complete

      // copy the packet DMX frame to the actual UART tx frame.  the UART tx
      // frame is larger to ensure enough bytes are sent to minimize flicker
      // for headunits that turn off between frames.

      size_t bytes = uart_write_bytes_with_break( // write the frame to the uart
          uart_num,                               // iart_num
          frame.data(),                           // data to send
          frame.size(),                           // number of bytes
          FRAME_BREAK);                           // bits to send as frame break

      if (bytes == frame.size()) {
        self->stats.frame_count++;
      } else {
        self->stats.frame_shorts++;
      }
    }
  });
}

esp_err_t DMX::uartInit() {
  esp_err_t esp_rc = ESP_FAIL;

  if (uart_rc == ESP_OK) {
    uart_config_t uart_conf = {};
    uart_conf.baud_rate = 250000;
    uart_conf.data_bits = UART_DATA_8_BITS;
    uart_conf.parity = UART_PARITY_DISABLE;
    uart_conf.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_conf.source_clk = UART_SCLK_APB;
    uart_conf.stop_bits = UART_STOP_BITS_2;

    if (esp_rc = uart_param_config(uart_num, &uart_conf); esp_rc != ESP_OK) {
      ESP_LOGW(pcTaskGetTaskName(nullptr), "[%s] uart_param_config()", esp_err_to_name(esp_rc));
    }

    uart_set_pin(uart_num, TX_PIN, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // this sequence is not part of the DMX512 protocol.  rather, these bytes
    // are sent to identify initiialization when viewing the serial data on
    // an oscillioscope.
    const char init_bytes[] = {0xAA, 0x55, 0xAA, 0x55};
    const size_t len = sizeof(init_bytes);
    uart_write_bytes_with_break(uart_num, init_bytes, len, (FRAME_BREAK * 2));
  }

  return esp_rc;
}

} // namespace ruth