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
#include <freertos/task.h>
#include <soc/uart_reg.h>

#include <lwip/err.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>

#include "dmx/dmx.hpp"

using namespace std;

namespace dmx {

Dmx *Dmx::_instance = nullptr;
TaskHandle_t _task_handle = nullptr;
static constexpr gpio_num_t _tx_pin = GPIO_NUM_17;

Dmx::Dmx(const uint32_t dmx_port) : _udp_port(dmx_port), _uart_num(UART_NUM_1) {
  _init_rc = uart_driver_install(_uart_num, 129, _tx_buff_len, 0, NULL, 0);
  _init_rc = uartInit();

  _frame.fill(0x00);

  _instance = this;
}

Dmx::~Dmx() {
  stop();
  // note:  the destructor must be called by a separate task
  while (_task_handle != nullptr) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

IRAM_ATTR void Dmx::fpsCalculate(void *data) {
  Dmx *dmx = (Dmx *)data;
  const auto mark = dmx->_frame_count_mark;
  const auto count = dmx->_stats.frame.count;

  if (mark && count) {
    dmx->_fpcp = count - mark;
    auto fps = (float)dmx->_fpcp / (float)dmx->_fpc_period;

    dmx->_stats.fps = fps;
  }

  dmx->_frame_count_mark = count;
}

void Dmx::stop() {
  if (_socket != -1) {
    shutdown(_socket, 0);
    close(_socket);
  }

  _mode = SHUTDOWN;
  vTaskDelay(pdMS_TO_TICKS(250));
}

void Dmx::taskCore(void *task_instance) {
  Dmx *dmx = (Dmx *)task_instance;

  dmx->taskInit();
  dmx->taskLoop(); // returns when _mode == SHUTDOWN

  // the task is complete, clean up
  TaskHandle_t task = _task_handle;
  _task_handle = nullptr;

  vTaskDelete(task); // task is removed the scheduler
}

void Dmx::taskInit() {
  int addr_family = AF_INET;
  int ip_protocol = 0;
  struct sockaddr_in6 dest_addr;

  struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
  dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
  dest_addr_ip4->sin_family = addr_family;
  dest_addr_ip4->sin_port = htons(_udp_port);
  ip_protocol = IPPROTO_IP;

  _socket = socket(addr_family, SOCK_DGRAM, ip_protocol);

  if (_socket < 0) return;

  esp_timer_create_args_t timer_args = {};
  timer_args.callback = fpsCalculate;
  timer_args.arg = this;
  timer_args.dispatch_method = ESP_TIMER_TASK;
  timer_args.name = "dmx_fps";

  _init_rc = esp_timer_create(&timer_args, &_fps_timer);

  if (_init_rc == ESP_OK) {
    // the interval to count frames in µs
    // _fpc_period is in seconds
    _init_rc = esp_timer_start_periodic(_fps_timer, _fpc_period * 1000 * 1000);
  }
}

IRAM_ATTR void Dmx::taskLoop() {
  _mode = STREAM_FRAMES;

  // when _mode is SHUTDOWN this function returns
  while (_mode != SHUTDOWN) {
    Packet packet;
    constexpr size_t packet_max_len = sizeof(Packet);

    struct sockaddr_storage source_addr;
    socklen_t socklen = sizeof(source_addr);
    int len = recvfrom(_socket, &packet, packet_max_len - 1, 0, (struct sockaddr *)&source_addr, &socklen);

    if ((len > 0) && packet.validMagic()) {
      memcpy(_frame.data(), packet.frameData(), packet.frameDataLength());

      txFrame(packet);

      if (packet.deserializeMsg()) {
        for (auto hu : _headunits) {
          hu->handleMsg(packet.rootObj());
        }
      }
    }
  }

  // run loop is has fallen through, shutdown the task
  esp_timer_stop(_fps_timer);

  vTaskDelay(pdMS_TO_TICKS(1));

  if (uart_is_driver_installed(_uart_num)) {
    uart_driver_delete(_uart_num);

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  esp_timer_delete(_fps_timer);
  _fps_timer = nullptr;
}

void Dmx::taskStart() {
  if (_task_handle == nullptr) {
    // this (object) is passed as the data to the task creation and is
    // used by the static coreTask method to call cpre()
    ::xTaskCreate(&taskCore, "Rdmx", 4096, this, 19, &(_task_handle));
  }
}

IRAM_ATTR void Dmx::txFrame(const dmx::Packet &packet) {
  // wait up to the max time to transmit a TX frame
  const TickType_t uart_wait_ms = (_frame_us / 1000) + 1;
  TickType_t frame_ticks = pdMS_TO_TICKS(uart_wait_ms);

  // always ensure the previous tx has completed which includes
  // the BREAK (low for 88us)
  if (uart_wait_tx_done(_uart_num, frame_ticks) == ESP_OK) {
    // at the end of the TX the UART pulls the TX low to generate the BREAK
    // once the code reaches this point the BREAK is complete

    // copy the packet DMX frame to the actual UART tx frame.  the UART tx
    // frame is larger to ensure enough bytes are sent to minimize flicker
    // for headunits that turn off between frames.

    const uint8_t *packet_frame = packet.frameData();
    for (uint32_t k = 0; k < packet.frameDataLength(); k++) {
      _frame[k] = packet_frame[k];
    }

    size_t bytes = uart_write_bytes_with_break(_uart_num, _frame.data(), _frame.size(), _frame_break);

    if (bytes == _frame.size()) {
      _stats.frame.count++;
    } else {
      _stats.frame.shorts++;
    }
  }
}

esp_err_t Dmx::uartInit() {
  esp_err_t esp_rc = ESP_FAIL;

  if (_init_rc == ESP_OK) {
    uart_config_t uart_conf = {};
    uart_conf.baud_rate = 250000;
    uart_conf.data_bits = UART_DATA_8_BITS;
    uart_conf.parity = UART_PARITY_DISABLE;
    uart_conf.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_conf.source_clk = UART_SCLK_APB;
    uart_conf.stop_bits = UART_STOP_BITS_2;

    if ((esp_rc = uart_param_config(_uart_num, &uart_conf)) != ESP_OK) {
      ESP_LOGW(pcTaskGetTaskName(nullptr), "[%s] uart_param_config()", esp_err_to_name(esp_rc));
    };

    uart_set_pin(_uart_num, _tx_pin, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // this sequence is not part of the DMX512 protocol.  rather, these bytes
    // are sent to identify initiialization when viewing the serial data on
    // an oscillioscope.
    const char init_bytes[] = {0xAA, 0x55, 0xAA, 0x55};
    const size_t len = sizeof(init_bytes);
    uart_write_bytes_with_break(_uart_num, init_bytes, len, (_frame_break * 2));
  }

  return esp_rc;
}

} // namespace dmx
