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

#include "binder/binder.hpp"
#include "io/io.hpp"
#include "misc/elapsed.hpp"
#include "ru_base/types.hpp"

#include <cstdint>
#include <esp_https_ota.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace ruth {
namespace desk {

class MsgOut;

class OTA : public std::enable_shared_from_this<OTA> {
private:
  typedef enum : uint8_t { INIT = 0x01, EXECUTE, CANCEL, FINISH, ERROR } state_t;

private:
  OTA(tcp_socket &&sock, const char *url, const char *file) noexcept;

public:
  ~OTA() noexcept;

  inline static auto create(auto &&sock, const auto url, const auto file) noexcept {
    auto self = std::shared_ptr<OTA>(new OTA(std::forward<decltype(sock)>(sock), url, file));

    return self->shared_from_this();
  }

  void execute() noexcept;

  static void validate_pending(Binder *binder);

private:
  bool check_error(esp_err_t esp_rc, const char *details) noexcept;

  void download() noexcept;

  void finish() noexcept;

  state_t initialize() noexcept;
  bool is_same_image(const esp_app_desc_t *a, esp_app_desc_t *b) noexcept;

  static void mark_valid(TimerHandle_t h) noexcept;

  static void restart(TimerHandle_t h) noexcept {
    xTimerDelete(h, pdMS_TO_TICKS(1000));

    esp_restart();
  }

  void send_response(MsgOut &&msg) noexcept;

private:
  // order dependent for object initialization
  tcp_socket sock;
  string url;
  state_t state;

  // order independent
  esp_https_ota_handle_t ota_handle;

public:
  string version;
  string error;
  Elapsed e;

public:
  static constexpr auto TAG{"OTA"};
  static constexpr size_t URL_MAX_LEN{512};
};

} // namespace desk
} // namespace ruth
