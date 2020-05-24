/*
    reading.cpp - Readings used within Ruth
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

#include <string>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <sys/time.h>
#include <time.h>

#include "net/network.hpp"
#include "protocols/mqtt.hpp"
#include "readings/reading.hpp"

namespace ruth {

void Reading::commonJSON(JsonDocument &doc) {
  doc["host"] = Net::hostID();
  doc["name"] = Net::hostname();
  doc["mtime"] = _mtime;
  doc["type"] = typeString(_type);

  if (_id.length() > 0) {
    doc["device"] = _id;
  }

  if (_cmd_ack) {
    doc["cmdack"] = _cmd_ack;
    doc["latency_us"] = _latency_us;
    doc["refid"] = _refid;
  }

  if (_log_reading) {
    doc["log_reading"] = true;
  }

  if (_crc_mismatches > 0) {
    doc["crc_mismatches"] = _crc_mismatches;
  }

  if (_read_errors > 0) {
    doc["read_errors"] = _read_errors;
  }

  if (_write_errors > 0) {
    doc["write_errors"] = _write_errors;
  }

  if (_read_us > 0) {
    doc["read_us"] = _read_us;
    doc["dev_latency_us"] = _read_us;
  }

  if (_write_us > 0) {
    doc["write_us"] = _write_us;
  }
}

string_t *Reading::json(char *buffer, size_t len) {
  // FUTURE IMPROVEMENT
  // std::unique_ptr<string_t> json_S(new string_t());
  // json_S.get()->reserve(1024);
  string_t *json_string = new string_t;
  json_string->reserve(2048);

  DynamicJsonDocument doc(2048);

  commonJSON(doc);
  populateJSON(doc);

  serializeMsgPack(doc, *json_string);

  return json_string;
}

void Reading::publish() { MQTT::publish(this); }

} // namespace ruth
