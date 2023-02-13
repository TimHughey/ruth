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

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <freertos/message_buffer.h>
#include <iterator>
#include <optional>
#include <soc/uart_periph.h>
#include <soc/uart_reg.h>

#include "dmx/dmx.hpp"
#include "dmx/frame.hpp"
#include "io/io.hpp"
#include "ru_base/time.hpp"

namespace ruth {

DRAM_ATTR static StaticTask_t tcb;
DRAM_ATTR static std::array<StackType_t, 3072> stack;
DRAM_ATTR static StaticMessageBuffer_t mbcb; // message buffer control block
DRAM_ATTR static std::array<uint8_t, dmx::FRAME_LEN * 3> mb_store;

DRAM_ATTR TaskHandle_t task_handle{nullptr};

// forward decl of static functions for DMX
static void run(void *data);
static bool uart_init();

// use a static run function because we're running a pure FreeRTOS
// task (not a fancy Thread)
void IRAM_ATTR run(void *data) {
  auto dmx = static_cast<DMX *>(data);

  ESP_LOGI(DMX::TAG.data(), "requested, run task_handle=%p", task_handle);

  dmx->spool_frames();

  ESP_LOGI(DMX::TAG.data(), "completed, deleting task handle=%p", task_handle);

  task_handle = nullptr;
  auto th = task_handle;

  vTaskDelete(th);
}

DRAM_ATTR static constexpr int UART_NUM{1}; // UART0=console, UART2=defect (use UART1)
DRAM_ATTR static constexpr gpio_num_t TX_PIN{GPIO_NUM_17};
DRAM_ATTR static constexpr uint32_t FRAME_BREAK{22}; // num bits at 250,000 baud (8µs)

bool uart_init() {
  static esp_err_t uart_rc{ESP_FAIL};
  constexpr size_t UART_MIN_BUFF{UART_FIFO_LEN + 1};
  constexpr size_t UART_TX_BUFF{std::max(dmx::FRAME_LEN, UART_MIN_BUFF)};

  if (uart_rc == ESP_OK) { // successfully installed
    return false;
  }

  // uart driver not installed or failed last time
  auto flags = ESP_INTR_FLAG_IRAM;
  if (uart_rc = uart_driver_install(UART_NUM, UART_MIN_BUFF, UART_TX_BUFF, 0, NULL, flags);
      uart_rc == ESP_OK) {

    const uart_config_t uart_conf = {.baud_rate = 250000,
                                     .data_bits = UART_DATA_8_BITS,
                                     .parity = UART_PARITY_DISABLE,
                                     .stop_bits = UART_STOP_BITS_2,
                                     .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                                     .rx_flow_ctrl_thresh = 0,
                                     .source_clk = UART_SCLK_APB};

    if (uart_rc = uart_param_config(UART_NUM, &uart_conf); uart_rc != ESP_OK) {
      ESP_LOGW(pcTaskGetName(nullptr), "[%s] uart_param_config()", esp_err_to_name(uart_rc));
    }

    uart_set_pin(UART_NUM, TX_PIN, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // this sequence is not part of the DMX512 protocol.  rather, these bytes
    // are sent to identify initiialization when viewing the serial data on
    // an oscillioscope.
    const char init_bytes[] = {0xAA, 0x55, 0xAA, 0x55};
    const size_t len = sizeof(init_bytes);
    uart_write_bytes_with_break(UART_NUM, init_bytes, len, (FRAME_BREAK * 2));
  }

  return uart_rc == ESP_OK;
}

// object API

DMX::DMX() noexcept : qok{0}, qrf(0), qsf(0) {
  // wait for previous DMX task to stop, if there is one
  for (auto waiting_ms = 0; task_handle; waiting_ms++) {
    vTaskDelay(pdMS_TO_TICKS(1));

    if ((waiting_ms % 100) == 0) {
      ESP_LOGW(TAG.data(), "waiting to start task, %dms", waiting_ms);
    }
  }

  msg_buff = xMessageBufferCreateStatic(mb_store.size(), mb_store.data(), &mbcb);

  uart_init(); // only inits uart once

  task_handle =                       // create the task using a static stack
      xTaskCreateStatic(&run,         // static func to start task
                        TAG.data(),   // task name
                        stack.size(), // stack size
                        this,         // pass the newly created object to run()
                        5,            // priority
                        stack.data(), // static task stack
                        &tcb          // task control block
      );

  ESP_LOGD(TAG.data(), "init, obj=%p tcb=%p", this, &tcb);
}

DMX::~DMX() noexcept {
  vMessageBufferDelete(msg_buff);

  while (task_handle != nullptr) {
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  ESP_LOGD(TAG.data(), "falling out of scope");
}

void IRAM_ATTR DMX::spool_frames() noexcept {

  while (shutdown_prom.has_value() == false) {
    dmx::frame frame;

    // always ensure the previous tx has completed which includes
    // the BREAK (low for 88us)
    if (ESP_OK != uart_wait_tx_done(UART_NUM, FRAME_TICKS)) {
      ESP_LOGW(TAG.data(), "uart_wait_tx_done() timeout");
      continue;
    }

    auto msg_bytes = xMessageBufferReceive(msg_buff, frame.data(), frame.size(), RECV_TIMEOUT);

    if (msg_bytes) {

      qok++;
      // at the end of the TX the UART pulls the TX low to generate the BREAK
      // once the code reaches this point the BREAK is complete

      // copy the packet DMX frame to the actual UART tx frame.  the UART tx
      // frame is larger to ensure enough bytes are sent to minimize flicker
      // for headunits that turn off between frames.

      size_t bytes = uart_write_bytes_with_break( // write the frame to the uart
          UART_NUM,                               // uart_num
          frame.data(),                           // data to send
          frame.size(),                           // number of bytes
          FRAME_BREAK);                           // bits to send as frame break

      if (bytes != frame.size()) {
        ESP_LOGW(TAG.data(), "short frame, frame_bytes=%u tx_bytes=%u", frame.size(), bytes);
      }
    } else {
      qrf++;
    }
  }

  shutdown_prom->set_value(true);
}

bool IRAM_ATTR DMX::tx_frame(dmx::frame &&frame) {
  // wait up to the max time to transmit a TX frame
  auto msg_bytes = xMessageBufferSend(msg_buff, frame.data(), frame.len, QUEUE_TICKS);

  if (msg_bytes != frame.len) qsf++;

  return msg_bytes == frame.len;
}

} // namespace ruth