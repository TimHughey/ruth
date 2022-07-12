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

#include <chrono>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <freertos/task.h>

#include <array>
#include <iterator>
#include <lwip/err.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <memory>
#include <mutex>
#include <soc/uart_periph.h>
#include <soc/uart_reg.h>
#include <vector>

using namespace std::literals::chrono_literals;

namespace ruth {

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

DMX::DMX(io_context &io_ctx)
    : fps_timer(io_ctx) // frames per second timer
{

  // uart driver not installed or failed last time
  if (uart_rc == ESP_FAIL) {
    if (uart_rc = uart_driver_install(uart_num, 129, TX_LEN, 0, NULL, 0); uart_rc == ESP_OK) {
      uart_rc = uartInit();
    }
  }
}

IRAM_ATTR void DMX::fpsCalculate() {

  if (state == desk::State::SHUTDOWN) {
    return;
  }

  fps_timer.expires_at(render_start + Millis(FRAME_STATS * stats.frame_count));

  fps_timer.async_wait([this](const error_code ec) {
    if (!ec) {

      if (frame_count_mark && stats.frame_count) {
        fpcp = stats.frame_count - frame_count_mark;

        auto fps = fpcp / FRAME_STATS.count();

        stats.fps = fps;
        ESP_LOGD("dmx", "fps=%2.2f", fps);
      }

      // save the frame count as of this invocation
      frame_count_mark = stats.frame_count;

      fpsCalculate(); // restart timer

    } else if (ec != asio::error::basic_errors::operation_aborted) {
      ESP_LOGD("DMX", "terminating resason=%s\n", ec.message().c_str());
    }
  });
}

void DMX::stop() {

  fps_timer.cancel(ec);

  state = Sate::SHUTDOWN;
}

IRAM_ATTR void DMX::renderLoop() {

  auto &rxb = rxBuffer();

  socket.async_receive_from(
      asio::buffer(rxb.data(), rxb.size()), [this](error_code ec, size_t rx_bytes) {
        if (!ec && rx_bytes) {

          std::copy(buff.begin(), buff.begin() + rx_bytes, std::back_inserter(packet));
        }
      });

  // when mode is SHUTDOWN this function returns
  auto rx_bytes = 0;
  while (mode != SHUTDOWN) {
    dmx::Packet packet;

    rx_bytes = recvfrom(socket, packet.rxData(), packet.maxRxLength(), 0, nullptr, nullptr);

    if ((rx_bytes > 0) && packet.validMagic()) {
      memcpy(frame.data(), packet.frameData(), packet.frameDataLength());
      txFrame(packet);

      if (packet.deserializeMsg()) {
        for (auto unit : head_units) {
          unit->handleMsg(packet.rootObj());
        }
      }
    }
  }

  // run loop is falling through
}

IRAM_ATTR void DMX::txFrame(const dmx::Packet &packet) {
  // wait up to the max time to transmit a TX frame
  TickType_t frame_ticks = pdMS_TO_TICKS(FRAME_MS.count());

  // always ensure the previous tx has completed which includes
  // the BREAK (low for 88us)
  if (uart_wait_tx_done(uart_num, frame_ticks) == ESP_OK) {
    // at the end of the TX the UART pulls the TX low to generate the BREAK
    // once the code reaches this point the BREAK is complete

    // copy the packet DMX frame to the actual UART tx frame.  the UART tx
    // frame is larger to ensure enough bytes are sent to minimize flicker
    // for headunits that turn off between frames.

    const uint8_t *packet_frame = packet.frameData();
    for (uint32_t k = 0; k < packet.frameDataLength(); k++) {
      frame[k] = packet_frame[k];
    }

    size_t bytes = uart_write_bytes_with_break(uart_num, frame.data(), frame.size(), FRAME_BREAK);

    if (bytes == frame.size()) {
      stats.frame_count++;
    } else {
      stats.frame_shorts++;
    }
  }
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