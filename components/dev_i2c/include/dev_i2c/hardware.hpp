/*
    Ruth
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

#ifndef ruth_i2c_hardware_hpp
#define ruth_i2c_hardware_hpp

#include <memory>

#include <esp_err.h>
#include <freertos/FreeRTOS.h>

namespace i2c {
class Hardware {
public:
  enum Notifies : uint32_t { BUS_NEEDED = 0xb000, BUS_RELEASED = 0xb001 };

public:
  struct DataRequest {
    uint8_t addr;
    float timeout_scale = 1.0;
    struct {
      uint8_t *data;
      size_t len;
    } tx;

    struct {
      uint8_t *data;
      size_t len;
    } rx;
  };

public:
  Hardware(const uint8_t addr);

  uint8_t addr() const { return _addr; }

  const char *ident() const { return _ident; }
  static size_t identMaxLen() { return _ident_max_len; }
  static bool initHardware();
  static uint8_t lastStatus();
  static uint64_t now();
  static bool ok();

  static void setUniqueId(const char *id);
  static void setReportFrequency(uint32_t micros);

protected:
  typedef uint8_t *Bytes;
  typedef const size_t Len;

protected:
  static bool acquireBus(uint32_t timeout_ms = UINT32_MAX);

  static bool releaseBus();
  static bool requestData(const DataRequest &request);
  static bool resetBus();

private:
  static bool ensureBus();
  void makeID();

private:
  static constexpr size_t _ident_max_len = 45;

  uint8_t _addr;
  char _ident[_ident_max_len];
};
} // namespace i2c

#endif // pwm_dev_hpp
