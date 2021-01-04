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

namespace ruth {

static Dmx_t *__singleton__ = nullptr;

// SINGLETON! constructor is private
Dmx::Dmx() {
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

  if (_init_rc == ESP_OK) {
    timer_args.callback = frameTimerCallback;
    timer_args.name = "dmx frame";

    _init_rc = esp_timer_create(&timer_args, &_frame_timer);
  }

  _init_rc = uart_driver_install(_uart_num, 129, _tx_buff_len, 0, NULL, 0);
  _init_rc = uartInit();

  _init_rc = frameTimerStart();
}

void IRAM_ATTR Dmx::core() {
  while (_stream_frames) {

    uint32_t val;
    auto notified =
        xTaskNotifyWait(0x00, ULONG_MAX, &val, pdMS_TO_TICKS(portMAX_DELAY));

    if (notified == pdPASS) {

      const NotifyVal_t notify_val = static_cast<NotifyVal_t>(val);

      switch (notify_val) {
      case NotifyFrame:
        txFrame();
        break;

      default:
        _stats.notify_failures++;
        break;
      }
    }
  }
}

void Dmx::coreTask(void *task_instance) {
  Dmx_t *dmx = (Dmx_t *)task_instance;

  dmx->core();

  // if the core task ever returns then wait forever
  vTaskDelay(UINT32_MAX);
}

void Dmx::fpsCalculate(void *data) {
  Dmx_t *dmx = (Dmx_t *)data;
  const auto mark = dmx->_frame_count_mark;
  const auto count = dmx->_stats.frames;

  if (mark && count) {
    dmx->_fpcp = count - mark;
    dmx->_stats.fps = (float)dmx->_fpcp / (float)dmx->_fpc_period;
  }

  dmx->_frame_count_mark = count;
}

void Dmx::frameTimerCallback(void *data) {
  Dmx_t *dmx = (Dmx_t *)data;

  dmx->taskNotify(NotifyFrame);
}

esp_err_t Dmx::frameTimerStart() const {
  esp_err_t rc = _init_rc;

  if (_init_rc == ESP_OK) {
    rc = esp_timer_start_once(_frame_timer, _frame_us);
  }

  return rc;
}

void Dmx::pause() { _paused = true; }

void Dmx::registerHeadUnit(HeadUnit_t *unit) {
  if (_headunits < 10) {
    _headunit[_headunits] = unit;
    _headunits++;
  }
}

void Dmx::resume() {
  _paused = false;
  frameTimerStart();
}

void Dmx::shutdown() {
  Dmx_t *dmx = instance();

  dmx->_stream_frames = false;

  esp_timer_stop(dmx->_fps_timer);
  esp_timer_delete(dmx->_fps_timer);
  dmx->_fps_timer = nullptr;
}

void IRAM_ATTR Dmx::txFrame() {

  if (_paused) {
    return;
  }

  // wait up to the max time to transmit a TX frame
  const TickType_t uart_wait_ms = (_frame_us / 1000) + 1;
  TickType_t frame_ticks = pdMS_TO_TICKS(uart_wait_ms);

  // always ensure the previous tx has completed which includes
  // the BREAK (low for 88us)
  if (uart_wait_tx_done(_uart_num, frame_ticks) == ESP_OK) {
    // at the end of the TX the UART pulls the TX low to generate the BREAK
    // once the code reaches this point the BREAK is complete

    _tx_elapsed.reset();

    frameTimerStart();

    auto bytes = uart_write_bytes_with_break(_uart_num, (const char *)_frame,
                                             _frame_len, _frame_break);

    _tx_elapsed.freeze(); // stop TX elapsed time tracking

    if (bytes == _frame_len) {
      _stats.frames++;
    } else {
      _stats.tx_short_frames++;
    }

    // update the next frame with registered head unit staged frame updates
    for (auto i = 0; i < _headunits; i++) {
      HeadUnit_t *unit = _headunit[i];

      if (unit) {
        unit->updateFrame(_frame);
      }
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

    if ((esp_rc = uart_set_mode(_uart_num, UART_MODE_RS485_HALF_DUPLEX)) !=
        ESP_OK) {
      ESP_LOGW(pcTaskGetTaskName(nullptr), "[%s] uart_set_mode()\n",
               esp_err_to_name(esp_rc));
    };

    uart_set_pin(_uart_num, _tx_pin, 16, UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);

    // this sequence is not part of the DMX512 protocol.  rather, these bytes
    // sent to identify initiialization when viewing the serial data on
    // an oscillioscope.
    const char init_bytes[] = {0xAA, 0x55, 0xAA, 0x55};
    const size_t len = sizeof(init_bytes);
    uart_write_bytes_with_break(_uart_num, init_bytes, len, _frame_break);
  }

  return esp_rc;
}

Dmx_t *Dmx::instance() {
  if (__singleton__ == nullptr) {
    __singleton__ = new Dmx();
  }

  return __singleton__;
}

} // namespace ruth
