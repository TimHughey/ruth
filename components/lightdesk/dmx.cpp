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
#include "io/io.hpp"
#include "ru_base/time.hpp"

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <iterator>
#include <optional>
#include <soc/uart_periph.h>
#include <soc/uart_reg.h>

using namespace std::literals::chrono_literals;

namespace ruth {

DRAM_ATTR static StaticTask_t tcb;
DRAM_ATTR static std::array<StackType_t, 3072> stack;
TaskHandle_t task_handle = nullptr;

static constexpr csv TAG = "dmx";

// forward decl of static functions for DMX
static void run(void *data);
static bool uart_init();

void run(void *data) {
  std::optional<shDMX> *sh_dmx = static_cast<std::optional<shDMX> *>(data);
  auto self = sh_dmx->value()->shared_from_this();

  self->fpsCalculate();
  self->io_ctx.run();

  auto task_to_delete = task_handle;
  task_handle = nullptr;

  ESP_LOGI(TAG.data(), "run() io_ctx work exhausted");

  vTaskDelete(task_to_delete);
  sh_dmx->reset();
}

constexpr int UART_NUM = 1; // UART0 is the console, UART2 has a defect... use UART1
constexpr gpio_num_t TX_PIN = GPIO_NUM_17;
constexpr uint_fast32_t FRAME_BREAK = 22; // num bits at 250,000 baud (8µs)

bool uart_init() {
  static esp_err_t uart_rc = ESP_FAIL;

  // uart driver not installed or failed last time
  if (uart_rc == ESP_FAIL) {
    if (uart_rc = uart_driver_install(uart_num, 129, DMX::Frame::TX_LEN, 0, NULL, 0);
        uart_rc == ESP_OK) {

      uart_config_t uart_conf = {};
      uart_conf.baud_rate = 250000;
      uart_conf.data_bits = UART_DATA_8_BITS;
      uart_conf.parity = UART_PARITY_DISABLE;
      uart_conf.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
      uart_conf.source_clk = UART_SCLK_APB;
      uart_conf.stop_bits = UART_STOP_BITS_2;

      if (uart_rc = uart_param_config(uart_num, &uart_conf); uart_rc != ESP_OK) {
        ESP_LOGW(pcTaskGetTaskName(nullptr), "[%s] uart_param_config()", esp_err_to_name(uart_rc));
      }

      uart_set_pin(UART_NUM, TX_PIN, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

      // this sequence is not part of the DMX512 protocol.  rather, these bytes
      // are sent to identify initiialization when viewing the serial data on
      // an oscillioscope.
      const char init_bytes[] = {0xAA, 0x55, 0xAA, 0x55};
      const size_t len = sizeof(init_bytes);
      uart_write_bytes_with_break(uart_num, init_bytes, len, (FRAME_BREAK * 2));
    }
  }

  return uart_rc == ESP_OK;
}

// DMX frames per second
using FPS = std::chrono::duration<float, std::ratio<1, 44>>;

constexpr Micros FRAME_MAB = 12us;
constexpr Micros FRAME_BYTE = 44us;
constexpr Micros FRAME_SC = FRAME_BYTE;
constexpr Micros FRAME_MTBF = 44us;
constexpr Micros FRAME_DATA = Micros(FRAME_BYTE * 512);
constexpr Millis FRAME_STATS = Millis(2s);
constexpr Seconds FRAME_STATS_SECS = 2s;

// frame interval does not include the BREAK as it is handled by the UART
constexpr Micros FRAME_US = FRAME_MAB + FRAME_SC + FRAME_DATA + FRAME_MTBF;
constexpr Millis FRAME_MS = ru_time::as_duration<Micros, Millis>(FRAME_US);

DMX::DMX() : fps_timer(io_ctx) { uart_init(); }

IRAM_ATTR void DMX::fpsCalculate() {

  fps_timer.expires_at(stats.fps_start + Millis(FRAME_STATS * stats.calcs++));
  fps_timer.async_wait([this](const error_code ec) {
    if (!ec) { // success
      if (stats.mark && stats.frame_count) {
        // enough info to calc fps
        stats.fps = (stats.frame_count - stats.mark) / FRAME_STATS_SECS.count();
        ESP_LOGI(TAG.data(), "fps=%2.2f", stats.fps);
      }

      // always save the frame count (which could be zero)
      stats.mark = stats.frame_count;

      fpsCalculate(); // restart timer
    } else if (ec == io::aborted) {
      ESP_LOGD(TAG.data(), "fpsCalculate() timer restart");
    } else {
      ESP_LOGW(TAG.data(), "fpsCalculate() terminating reason=%s", ec.message().c_str());
    }
  });
}

// static create and reset

shDMX DMX::start() { // static
  static std::optional<shDMX> sh_dmx;

  if (!sh_dmx.has_value()) {
    sh_dmx.emplace(new DMX());

    task_handle =                       // create the task using a static stack
        xTaskCreateStatic(&run,         // static func to start task
                          TAG.data(),   // task name
                          stack.size(), // stack size
                          &sh_dmx,      // task data
                          15,           // priority
                          stack.data(), // static task stack
                          &tcb);        // task control block

    ESP_LOGI(TAG.data(), "created=%p started tcb=%p", sh_dmx.value().get(), &tcb);
  }

  return sh_dmx.value()->shared_from_this();
}

IRAM_ATTR void DMX::txFrame(DMX::Frame &&frame) {
  // wait up to the max time to transmit a TX frame
  static TickType_t frame_ticks = pdMS_TO_TICKS(FRAME_MS.count());

  frame.preventFlicker();

  asio::post(io_ctx, [self = shared_from_this(), frame = std::move(frame)]() mutable {
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

} // namespace ruth