/*
    ota.hpp - Ruth Command OTA Class
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

#ifndef ruth_cmd_ota_h
#define ruth_cmd_ota_h

#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_spi_flash.h>

#include "cmds/base.hpp"

using std::unique_ptr;

namespace ruth {

typedef class CmdOTA CmdOTA_t;
class CmdOTA : public Cmd {
private:
  string_t _uri;

  void doUpdate();

  static esp_err_t httpEventHandler(esp_http_client_event_t *evt);

public:
  CmdOTA(JsonDocument &doc, elapsedMicros &parse);
  ~CmdOTA(){};

  static void markPartitionValid();

  bool process();
  size_t size() const { return sizeof(CmdOTA_t); };

  const unique_ptr<char[]> debug();
};

} // namespace ruth

#endif
