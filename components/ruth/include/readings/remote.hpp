/*
    remote_reading.hpp - Ruth Celsius Reading
    Copyright (C) 2017  Tim Hughey

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

#ifndef _ruth_reading_remote_hpp
#define _ruth_reading_remote_hpp

#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <sys/time.h>
#include <time.h>

#include "readings/reading.hpp"

using std::unique_ptr;

namespace ruth {
namespace reading {
typedef class Remote Remote_t;
typedef unique_ptr<Remote_t> Remote_ptr_t;

class Remote : public Reading_t {

public:
  Remote();
  Remote(ReadingType_t type);

protected:
  virtual void populateMessage(JsonDocument &doc);

private:
  wifi_ap_record_t ap_;
  TextBuffer<17> bssid_;
  esp_err_t ap_rc_;
  uint32_t heap_free_;
  uint32_t heap_min_;
  uint64_t uptime_us_;

private:
  void grabMetrics();
};
} // namespace reading
} // namespace ruth

#endif
