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
#include <soc/i2s_reg.h>

#include "protocols/i2s.hpp"

namespace ruth {

I2s::I2s() {}

I2s::~I2s() {
  if (_raw) {
    delete _raw;
    _raw = nullptr;
  }
}

void IRAM_ATTR I2s::filterNoise() {
  _noise = false;

  const size_t num_filters = sizeof(_noise_filters) / (sizeof(float) * 3);
  for (auto k = 0; k < num_filters; k++) {
    const float low = _noise_filters[k][0];
    const float high = _noise_filters[k][1];
    const float mag = _noise_filters[k][2];

    if (((_mpeak >= low) && (_mpeak < high)) && (_mpeak_mag < mag)) {
      _noise = true;
      break;
    }
  }
}

void IRAM_ATTR I2s::fft(uint8_t *buffer, size_t len) {
  elapsedMicros e;
  const size_t raw_samples = _vsamples_chan;
  const size_t num_int32 = len / sizeof(int32_t);
  static size_t raw_count = 0;

  // populate vreal left and right channels
  int32_t *val32 = (int32_t *)buffer;

  // split left and right data into separate fft datasets
  for (auto j = 0; j < num_int32; j = j + 2) {
    // shift to 24 bits
    const int32_t left = val32[j] >> 8;
    const int32_t right = val32[j + 1] >> 8;

    _vreal_left[raw_count] = left;
    _vreal_right[raw_count] = right;

    trackValMinMax(left, right);

    raw_count++;
  }

  if (raw_count < raw_samples) {
    // need more data before FFT
    return;
  }

  // we have enough data, reset buffer count and do FFT
  raw_count = 0;

  // zero out vimag
  memset(_vimag, 0x00, sizeof(_vimag));

  _fft.setArrays(_vreal_left, _vimag);
  _fft.dcRemoval();
  _fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  _fft.compute(FFTDirection::Forward);
  _fft.complexToMagnitude();

  _fft.majorPeak(_mpeak, _mpeak_mag);
  _mpeak_mag /= 100000.0;

  filterNoise();

  fftRecordElapsed(e);
}

bool I2s::handleNotifications() {
  bool rc = false;
  uint32_t val;
  auto notified = xTaskNotifyWait(0x00, ULONG_MAX, &val, 0);

  if (notified == pdPASS) {
    rc = true;
    const NotifyVal_t notify_val = static_cast<NotifyVal_t>(val);

    switch (notify_val) {
    case NotifySamplePrint:
      _sample_print = true;
      _print_elapsed.reset();
      break;

    case NotifySampleStopPrint:
      _sample_print = false;
      break;

    case NotifyStop:
      _mode = STOP;
      taskNotify(NotifyShutdown);
      break;

    case NotifyShutdown:
      _mode = SHUTDOWN;
      i2s_driver_uninstall(_i2s_port);
      vTaskDelay(pdMS_TO_TICKS(1));

      break;

    default:
      break;
    }
  }

  return rc;
}

float IRAM_ATTR I2s::majorPeak(float &mpeak, float &mpeak_mag) {
  mpeak = 0.0;
  mpeak_mag = 0.0;

  if (_noise == false) {
    mpeak = _mpeak;
    mpeak_mag = _mpeak_mag;
  }

  return mpeak;
}

bool IRAM_ATTR I2s::samplesRx() {
  auto rc = false;
  auto esp_rc = ESP_OK;
  size_t bytes_read = 0;
  const TickType_t ticks = pdMS_TO_TICKS(20);
  uint8_t *buffer = _raw;

  elapsedMicros e;
  esp_rc = i2s_read(_i2s_port, buffer, _dma_buff, &bytes_read, ticks);
  samplesRxElapsed(e);

  if ((esp_rc == ESP_OK) && (bytes_read == _dma_buff)) {
    rc = true;

    // udpSend(buffer, bytes_read);

    // printf("%6lluµs ", (uint64_t)elapsed);

    // compute(buffer, bytes_read);
    fft(buffer, bytes_read);

  } else {
    ESP_LOGW("i2s", "esp err=%s bytes_read=%d", esp_err_to_name(esp_rc),
             bytes_read);
  }

  return rc;
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
      .sample_rate = 44100, // or 44100 if you like
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
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

  _init_rc = i2s_driver_install(_i2s_port, &i2s_config, _eq_max_depth, nullptr);
  if (_init_rc != ESP_OK) {
    ESP_LOGW("I2S", "failed installing driver: %s\n",
             esp_err_to_name(_init_rc));
  }

  _init_rc = i2s_set_pin(_i2s_port, &pin_config);
  if (_init_rc != ESP_OK) {
    ESP_LOGW("I2S", "failed setting pins: %s\n", esp_err_to_name(_init_rc));
  }

  if (_raw == nullptr) {
    _raw = new uint8_t[_dma_buff];
    bzero(_raw, _dma_buff);
  }
}

void IRAM_ATTR I2s::taskLoop() {
  _mode = PROCESS_AUDIO;

  // when _mode is SHUTDOWN this function returns
  while (_mode != SHUTDOWN) {
    handleNotifications();
    samplesRx();
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
