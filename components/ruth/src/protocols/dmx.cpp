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

#include "protocols/dmx.hpp"
#include "readings/text.hpp"

using namespace std;
using namespace asio;
using namespace ruth::dmx;

using asio::buffer;

namespace ruth {

Dmx *Dmx::_instance = nullptr;

Dmx::Dmx() {
  _init_rc = uart_driver_install(_uart_num, 129, _tx_buff_len, 0, NULL, 0);
  _init_rc = uartInit();

  _frame.fill(0x00);

  _instance = this;
}

Dmx::~Dmx() {
  stop();
  // note:  the destructor must be called by a separate task
  while (_task.handle != nullptr) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

IRAM_ATTR void Dmx::fpsCalculate(void *data) {
  Dmx_t *dmx = (Dmx_t *)data;
  const auto mark = dmx->_frame_count_mark;
  const auto count = dmx->_stats.frame.count;

  if (mark && count) {
    dmx->_fpcp = count - mark;
    auto fps = (float)dmx->_fpcp / (float)dmx->_fpc_period;

    dmx->_stats.fps = fps;
  }

  dmx->_frame_count_mark = count;
}

void Dmx::taskCore(void *task_instance) {
  Dmx_t *dmx = (Dmx_t *)task_instance;

  dmx->taskInit();
  dmx->taskLoop(); // returns when _mode == SHUTDOWN

  // the task is complete, clean up
  TaskHandle_t task = dmx->_task.handle;
  dmx->_task.handle = nullptr;

  using TR = reading::Text;
  TR::rlog("%s finished, calling vTaskDelete()", __PRETTY_FUNCTION__);

  vTaskDelete(task); // task is removed the scheduler
}

void Dmx::taskInit() {
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

    if (_server->receive(packet) && packet.validMagic()) {
      auto rx_frame = buffer(packet.frameData(), packet.frameDataLength());
      auto tx_frame = buffer(_frame);
      buffer_copy(tx_frame, rx_frame);

      txFrame(packet);

      if (packet.deserializeMsg()) {

        for (auto hu : _headunits) {
          hu->handleMsg(packet.rootObj());
        }
      }
    }
  }

  _server.reset();

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
  if (_task.handle == nullptr) {
    // this (object) is passed as the data to the task creation and is
    // used by the static coreTask method to call cpre()
    ::xTaskCreate(&taskCore, "Rdmx", _task.stackSize, this, _task.priority,
                  &(_task.handle));
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
    auto bytes = uart_write_bytes_with_break(
        _uart_num, packet.frameData(), packet.frameDataLength(), _frame_break);

    if (bytes == packet.frameDataLength()) {
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
      ESP_LOGW(pcTaskGetTaskName(nullptr), "[%s] uart_param_config()",
               esp_err_to_name(esp_rc));
    };

    uart_set_pin(_uart_num, _tx_pin, 16, UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);

    // this sequence is not part of the DMX512 protocol.  rather, these bytes
    // are sent to identify initiialization when viewing the serial data on
    // an oscillioscope.
    const char init_bytes[] = {0xAA, 0x55, 0xAA, 0x55};
    const size_t len = sizeof(init_bytes);
    uart_write_bytes_with_break(_uart_num, init_bytes, len, (_frame_break * 2));
  }

  return esp_rc;
}

} // namespace ruth
