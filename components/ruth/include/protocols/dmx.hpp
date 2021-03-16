/*
    protocols/dmx.hpp - Ruth Dmx Protocol Engine
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

#ifndef _ruth_dmx512_hpp
#define _ruth_dmx512_hpp

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <soc/uart_reg.h>

#include <array>
#include <asio.hpp>
#include <iostream>
#include <string>
#include <unordered_set>

#include "core/binder.hpp"
#include "external/ArduinoJson.h"
#include "lightdesk/headunit.hpp"
#include "local/types.hpp"
#include "misc/elapsed.hpp"

namespace ruth {

typedef class Dmx Dmx_t;
typedef class DmxClient DmxClient_t;

typedef std::array<uint8_t, 512> NetFrame;

class Dmx {
  using udp = asio::ip::udp;
  typedef std::error_code error_code;

  enum { _frame_len = 127, buff_max_len = 512 };
  typedef std::array<uint8_t, buff_max_len> data_array;
  typedef enum { INIT = 0x00, STREAM_FRAMES, SHUTDOWN } DmxMode_t;

public:
  struct Stats {
    float fps = 0.0;

    struct {
      uint64_t count = 0;
      uint64_t shorts = 0;
    } frame;
  };

  typedef struct Stats Stats_t;

  // ASIO
public:
  class Server {
  public:
    Server(asio::io_context &io_ctx, short port)
        : _socket(io_ctx, udp::endpoint(udp::v4(), port)) {}

    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    bool receive(data_array &recv_buf) {
      auto rc = true;
      error_code ec;
      _socket.receive_from(asio::buffer(recv_buf), _remote_endpoint, 0, ec);

      if (ec) {
        rc = false;
      }

      return rc;
    }

  private:
    udp::socket _socket;
    udp::endpoint _remote_endpoint;
  };

private:
  typedef struct {
    size_t begin;
    size_t len;
  } BufferLocation;

  struct {
    BufferLocation magic = {.begin = 0, .len = 2};
    BufferLocation dmx_frame_len = {.begin = 2, .len = 2};
    BufferLocation dmx_frame = {.begin = 4, .len = 0};
    BufferLocation msgpack = {.begin = 0, .len = 0};
  } buff_pos;

public:
  Dmx();
  ~Dmx();

  Dmx(const Dmx &) = delete;
  Dmx &operator=(const Dmx &) = delete;
  void addHeadUnit(lightdesk::spHeadUnit hu) { _headunits.insert(hu); }

  void clientRegister(DmxClient_t *client) {
    // SHORT TERM implementation, needs:
    //  1. check to prevent duplicate registrations
    //  2. unregister
    //  3. error when max clients exceeded
    // if (_clients < 10) {
    //   _client[_clients] = client;
    //   _clients++;
    // }
  }

  inline float fpsExpected() const {
    constexpr float seconds_us = 1000.0f * 1000.0f;
    const float frame_us = static_cast<float>(_frame_us);
    return (seconds_us / frame_us);
  }

  inline uint64_t frameInterval() const { return _frame_us; }
  inline float frameIntervalAsSeconds() {
    const float frame_us = static_cast<float>(_frame_us);
    const float frame_secs = (frame_us / (1000.0f * 1000.0f));

    return frame_secs;
  }

  uint16_t frameLen() { return shortVal(buff_pos.dmx_frame_len); }

  float framesPerSecond() const { return _stats.fps; }
  uint16_t magic() const { return shortVal(buff_pos.magic); }

  uint16_t shortVal(const BufferLocation &loc) const {
    auto pos = _frame.begin() + loc.begin;
    auto lsb = *pos++;
    auto msb = *pos++;
    uint16_t val = lsb + (msb << 8);

    return val;
  }

  // task control
  void start() { taskStart(); }
  void stop() {
    _mode = SHUTDOWN;
    vTaskDelay(pdMS_TO_TICKS(250));
  }

private:
  static void fpsCalculate(void *data);

  void txFrame();
  esp_err_t uartInit();

  // task implementation
  inline TaskHandle_t task() const { return _task.handle; }
  static void taskCore(void *task_instance);
  void taskInit();
  void taskLoop();
  void taskStart();

private:
  uint64_t _pin_sel = GPIO_SEL_17;
  gpio_config_t _pin_cfg = {};
  gpio_num_t _tx_pin = GPIO_NUM_17;
  int _uart_num = UART_NUM_1;
  esp_err_t _init_rc = ESP_FAIL;

  DmxMode_t _mode = INIT;
  // static const size_t _frame_len = 127;
  data_array _frame; // the DMX frame starts as all zeros

  // except for _frame_break all frame timings are in µs
  const uint_fast32_t _frame_break = 22; // num bits at 250,000 baud (8µs)
  const uint_fast32_t _frame_mab = 12;
  const uint_fast32_t _frame_byte = 44;
  const uint_fast32_t _frame_sc = _frame_byte;
  const uint_fast32_t _frame_mtbf = 44;
  const uint_fast32_t _frame_data = _frame_byte * 512;
  // frame interval does not include the BREAK as it is handled by the UART
  uint64_t _frame_us = _frame_mab + _frame_sc + _frame_data + _frame_mtbf;

  const size_t _tx_buff_len = (_frame_len < 128) ? 0 : _frame_len + 1;

  esp_timer_handle_t _fps_timer = nullptr;
  uint64_t _frame_count_mark = 0;
  int _fpc_period = 2; // period represents seconds to count frames
  int _fpcp = 0;       // frames per calculate period

  lightdesk::HeadUnitTracker _headunits;

  DmxClient *_client[10] = {};
  size_t _clients = 0;

  Stats_t _stats;

  asio::io_context _io_ctx;
  std::shared_ptr<Server> _server =
      std::make_shared<Server>(_io_ctx, Binder::dmxPort());

  Task_t _task = {
      .handle = nullptr, .data = nullptr, .priority = 19, .stackSize = 4096};
};

class DmxClient {
public:
  DmxClient() { registerSelf(); }
  DmxClient(const uint16_t address, size_t frame_len)
      : _address(address), _frame_len(frame_len) {
    registerSelf();
  }
  ~DmxClient() {}

public:
  virtual void framePrepare() { printf("%s\n", __PRETTY_FUNCTION__); }
  virtual void frameUpdate(uint8_t *frame_actual);

  static void setDmx(Dmx_t *dmx) { DmxClient::_dmx = dmx; }

protected:
  static float fps() {
    if (_dmx) {
      return _dmx->fpsExpected();
    } else {
      return 44.0;
    }
  }
  inline bool &frameChanged() { return _frame_changed; }
  inline uint8_t *frameData() { return _frame_snippet; }
  static Dmx_t *dmx() { return _dmx; }
  void registerSelf() { _dmx->clientRegister(this); }

private:
  // class members, defined and initialized in misc/statics.cpp
  static Dmx_t *_dmx;

  uint16_t _address = 0;

  bool _frame_changed = false;
  size_t _frame_len = 0;

  uint8_t _frame_snippet[10] = {};
};

} // namespace ruth

#endif
