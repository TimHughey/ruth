/*
     protocols/i2s.cpp - Ruth I2s
     Copyright (C) 2021  Tim Hughey

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

#include <math.h>

#include <esp_log.h>
#include <esp_netif.h>

#include "protocols/i2s.hpp"

namespace ruth {

I2s::I2s() {
  _stats.config.freq_bin_width = (float)_sample_rate / (float)_vsamples_chan;

  _mutex = xSemaphoreCreateMutexStatic(&_mutex_buff);
}

I2s::~I2s() {
  while (_task.handle != nullptr) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  i2s_driver_uninstall(_i2s_port);

  if (_raw) {
    delete _raw;
    _raw = nullptr;
  }

  if (_stats_timer) {
    esp_timer_delete(_stats_timer);
    _stats_timer = nullptr;
  }
}

void IRAM_ATTR I2s::calculateComplexity() {
  uint_fast32_t complexity = 0;
  for (const Peak_t &x : _peaks) {
    if (x.dB >= _complexity_dB_floor) {
      complexity++;
    } else {
      // peaks are sorted highest to lowest so we can safely
      // ignore the rest if this one fails the
      // the complexity threshold
      break;
    }
  }

  _complexity_mavg.addValue(complexity);
}

void IRAM_ATTR I2s::fft(uint8_t *buffer, size_t len) {
  const size_t raw_samples = _vsamples_chan;
  const size_t num_int32 = len / sizeof(int32_t);
  static DRAM_ATTR size_t raw_count = 0;
  static DRAM_ATTR float chan_left_sum = 0;
  static DRAM_ATTR float chan_right_sum = 0;

  // populate vreal left and right channels
  int32_t *val32 = (int32_t *)buffer;

  // split left and right data into separate fft datasets
  for (auto j = 0; j < num_int32; j = j + 2) {
    // shift to 24 bits
    const int32_t left = val32[j] >> 8;
    _vreal_left[raw_count] = left;
    chan_left_sum += left;

    _stats.raw_val_left.track(left);

    const int32_t right = val32[j + 1] >> 8;
    _vreal_right[raw_count] = right;
    chan_right_sum += right;

    _stats.raw_val_right.track(right);

    raw_count++;
  }

  if (raw_count < raw_samples) {
    // need more data before FFT
    return;
  }

  // we have enough data for FFT
  _stats.elapsed.fft_us.track();

  // save the extra loop by computing the mean for dc
  // removal using the sum created while receiving the
  // samples.
  const float chan_left_mean = chan_left_sum / (float)raw_count;

  // zero out vimag
  memset(_vimag, 0x00, sizeof(_vimag));

  _fft.process(_vreal_left, _vimag, chan_left_mean);

  // portENTER_CRITICAL_SAFE(&_spinlock);
  constexpr TickType_t wait_ticks = 3; // 1.5ms @ 500Hz
  if (xSemaphoreTake(_mutex, wait_ticks) == pdTRUE) {
    _peaks = _fft.peaks();
    calculateComplexity();

    _stats.rate.fft_per_sec.track();
    xSemaphoreGive(_mutex);
  } else {
    printf("%s mutex timeout\n", __PRETTY_FUNCTION__);
  }
  // portEXIT_CRITICAL_SAFE(&_spinlock);

  _stats.elapsed.fft_us.record();

  // we're finished with this buffer, reset tracking info
  raw_count = 0;
  chan_left_sum = 0;
  chan_right_sum = 0;
}

bool I2s::handleNotifications() {
  bool rc = false;
  uint32_t val;
  auto notified = xTaskNotifyWait(0x00, ULONG_MAX, &val, 0);

  if (notified == pdPASS) {
    rc = true;
    const NotifyVal_t notify_val = static_cast<NotifyVal_t>(val);

    switch (notify_val) {
    case NotifyStop:
      _mode = STOP;
      // prevent further stats timer callbacks
      if (_stats_timer) {
        esp_timer_stop(_stats_timer);
        vTaskDelay(pdMS_TO_TICKS(100)); // allow stats timer to stop
      }

      taskNotify(NotifyShutdown);
      break;

    case NotifyShutdown:
      _mode = SHUTDOWN;

      break;

    case NotifyStatsCalculate:
      statsCalculate();
      break;

    default:
      break;
    }
  }

  return rc;
}

bool IRAM_ATTR I2s::samplesRx() {
  auto rc = false;
  auto esp_rc = ESP_OK;
  size_t bytes_read = 0;
  const TickType_t ticks = pdMS_TO_TICKS(20);
  uint8_t *buffer = _raw;

  _stats.elapsed.rx_us.track();
  esp_rc = i2s_read(_i2s_port, buffer, _dma_buff, &bytes_read, ticks);
  _stats.elapsed.rx_us.record();
  _stats.rate.raw_bps.track(bytes_read);

  if ((esp_rc == ESP_OK) && (bytes_read == _dma_buff)) {
    rc = true;

    fft(buffer, bytes_read);

  } else {
    ESP_LOGW("i2s", "esp err=%s bytes_read=%d", esp_err_to_name(esp_rc),
             bytes_read);
  }

  return rc;
}

void IRAM_ATTR I2s::statsCalculate() {
  _stats.rate.raw_bps.calculate();
  _stats.rate.samples_per_sec =
      _stats.rate.raw_bps.current() / (sizeof(int32_t) * 4);
}

void I2s::taskCore(void *task_instance) {
  I2s_t *i2s = (I2s_t *)task_instance;

  i2s->taskInit();
  i2s->udpInit();

  i2s->taskLoop(); // returns when _mode == SHUTDOWN

  // the task is complete, clean up
  TaskHandle_t task = i2s->_task.handle;
  i2s->_task.handle = nullptr;

  vTaskDelete(task); // task is removed the scheduler
}

void I2s::taskInit() {
  static const i2s_config_t i2s_config = {
      .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = _sample_rate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 2,
      .dma_buf_len = _dma_buff,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
  };

  static const i2s_pin_config_t pin_config = {
      .bck_io_num = 25,                  // Serial Clock (SCK)
      .ws_io_num = 26,                   // Word Select (WS)
      .data_out_num = I2S_PIN_NO_CHANGE, // not used (only for speakers)
      .data_in_num = 34                  // Serial Data (SD)
  };

  uint_fast16_t retries = 0;
  while (_need_driver_install) {
    _init_rc =
        i2s_driver_install(_i2s_port, &i2s_config, _evq_max_depth, nullptr);
    if (_init_rc == ESP_OK) {
      _need_driver_install = false;
    } else {
      if (((retries++ > 2) % 1000000) == 0) {
        ESP_LOGW("I2S", "failed attempt %u installing driver: %s\n", retries,
                 esp_err_to_name(_init_rc));
      }
      vTaskDelay(pdMS_TO_TICKS(70));
    }
  }

  _init_rc = i2s_set_pin(_i2s_port, &pin_config);
  if (_init_rc != ESP_OK) {
    ESP_LOGW("I2S", "failed setting pins: %s\n", esp_err_to_name(_init_rc));
  }

  vTaskDelay(pdMS_TO_TICKS(250)); // allow microphone to wake up

  if (_raw == nullptr) {
    _raw = new uint8_t[_dma_buff];
    bzero(_raw, _dma_buff);
  }

  esp_timer_create_args_t timer_args = {};
  timer_args.callback = statsCalculateCallback;
  timer_args.arg = this;
  timer_args.dispatch_method = ESP_TIMER_TASK;
  timer_args.name = "dmx_fps";

  if (_init_rc == ESP_OK) {
    _init_rc = esp_timer_create(&timer_args, &_stats_timer);

    if (_init_rc == ESP_OK) {
      const uint64_t us = _stats_interval_secs * 1000 * 1000;
      _init_rc = esp_timer_start_periodic(_stats_timer, us);
    }
  }

  if (_init_rc != ESP_OK) {
    ESP_LOGW("I2s", "%s failed, %s", __PRETTY_FUNCTION__,
             esp_err_to_name(_init_rc));
  }
}

void IRAM_ATTR I2s::taskLoop() {
  _mode = PROCESS_AUDIO;

  // when _mode is SHUTDOWN this function returns
  while (_mode != SHUTDOWN) {
    samplesRx();
    handleNotifications();
  }
}

BaseType_t IRAM_ATTR I2s::taskNotify(NotifyVal_t nval) {
  const uint32_t val = static_cast<uint32_t>(nval);
  auto rc = true;

  if (task()) {
    rc = xTaskNotify(task(), val, eSetValueWithOverwrite);
  } else {
    rc = false;
  }
  return rc;
}

void I2s::taskStart() {
  if (_task.handle == nullptr) {
    // this (object) is passed as the data to the task creation and is
    // used by the static coreTask method to call cpre()
    ::xTaskCreate(&taskCore, "Ri2s", _task.stackSize, this, _task.priority,
                  &(_task.handle));
  }
}

bool I2s::udpInit() {
  bool rc = true;

  _dest_raw.sin_addr.s_addr = inet_addr(_host_ip);
  _dest_raw.sin_family = AF_INET;
  _dest_raw.sin_port = htons(_port_raw);

  memcpy(&_dest_text, &_dest_raw, sizeof(struct sockaddr_in));
  _dest_text.sin_port = htons(_port_text);

  _socket_raw = socket(_addr_family, SOCK_DGRAM, _ip_protocol);
  _socket_text = socket(_addr_family, SOCK_DGRAM, _ip_protocol);
  if ((_socket_raw < 0) || (_socket_text < 0)) {
    rc = false;
    printf("\nunable to create socket: errno %d", errno);
  }

  return rc;
}

void I2s::udpPrint() {
  // dump the raw data to the network
  if (_socket_text >= 0) {
    struct sockaddr *dest = (struct sockaddr *)&_dest_text;
    constexpr size_t addr_size = sizeof(struct sockaddr_in);
    sendto(_socket_text, _print_buffer.c_str(), _print_buffer.size(), 0, dest,
           addr_size);
  }
}

void I2s::udpSend(uint8_t *data, size_t len) {
  // dump the raw data to the network
  if (_socket_raw >= 0) {
    struct sockaddr *dest = (struct sockaddr *)&_dest_raw;
    const size_t addr_size = sizeof(struct sockaddr_in);
    auto err = sendto(_socket_raw, data, len, 0, dest, addr_size);

    if (err < 0) {
      _udp_errors++;
    }
  }
}

} // namespace ruth
