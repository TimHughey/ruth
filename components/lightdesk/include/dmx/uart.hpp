//  Ruth
//  Copyright (C) 2020  Tim Hughey
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

#pragma once

#include "ru_base/rut.hpp"

#include <algorithm> // std::max()
#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_err.h>
#include <esp_log.h>
#include <soc/uart_periph.h>
#include <soc/uart_reg.h>

namespace ruth {
namespace dmx {
constexpr int UART_NUM{1}; // UART0=console, UART1=useable, UART2=defect
DRAM_ATTR static constexpr uart_config_t uart_conf = {.baud_rate = 250000,
                                                      .data_bits = UART_DATA_8_BITS,
                                                      .parity = UART_PARITY_DISABLE,
                                                      .stop_bits = UART_STOP_BITS_2,
                                                      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                                                      .rx_flow_ctrl_thresh = 0,
                                                      .source_clk = UART_SCLK_APB};

inline bool uart_init(std::size_t frame_len) {
  constexpr gpio_num_t TX_PIN{GPIO_NUM_17};
  static esp_err_t uart_rc{ESP_FAIL};

  // per ESP-IDF docs:
  //  Rx_buffer_size should be greater than UART_FIFO_LEN.
  //  Tx_buffer_size should be either zero or greater than UART_FIFO_LEN.
  constexpr std::size_t UART_RX_BUFF{UART_FIFO_LEN + 1};
  const std::size_t UART_TX_BUFF{std::max(frame_len * 4, UART_RX_BUFF)};

  if (uart_rc == ESP_OK) return false; // installed previously, do nothing

  // uart driver not installed or failed last time
  auto flags = ESP_INTR_FLAG_IRAM;

  if (uart_rc = uart_driver_install(UART_NUM, UART_RX_BUFF, UART_TX_BUFF, 0, NULL, flags);
      uart_rc == ESP_OK) {

    if (uart_rc = uart_param_config(UART_NUM, &uart_conf); uart_rc != ESP_OK) {
      ESP_LOGW(pcTaskGetName(nullptr), "[%s] uart_param_config()", esp_err_to_name(uart_rc));
    }

    uart_set_pin(UART_NUM, TX_PIN, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  }

  return uart_rc == ESP_OK;
}
} // namespace dmx
} // namespace ruth