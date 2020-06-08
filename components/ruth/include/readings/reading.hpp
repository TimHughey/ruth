/*
    reading.h - Readings used within Ruth
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

#ifndef _ruth_reading_hpp
#define _ruth_reading_hpp

// #include <memory>
#include <string>

#include <external/ArduinoJson.h>
#include <sys/time.h>
#include <time.h>

#include "local/types.hpp"
#include "misc/elapsed.hpp"

namespace ruth {
namespace reading {

typedef class Reading Reading_t;
typedef std::unique_ptr<Reading_t> Reading_ptr_t;

class Reading {
protected:
  typedef enum {
    BASE = 0,
    REMOTE,
    SENSOR,
    BOOT,
    SWITCH,
    TEXT,
    PWM
  } ReadingType_t;

public:
  Reading(ReadingType_t type) : _type(type){};
  Reading(const string_t &id, ReadingType_t type) : _id(id), _type(type){};

  virtual ~Reading() {
    if (_json != nullptr) {
      delete _json;
    }
  };

  string_t *json(char *buffer = nullptr, size_t len = 0);
  virtual void publish();
  virtual void refresh() { time(&_mtime); }
  void setCmdAck(uint32_t latency_us, const RefID_t &refid) {
    _cmd_ack = true;
    _latency_us = latency_us;

    _refid = refid;
  }

  void setCmdAck(uint32_t latency_us, const char *refid) {
    _cmd_ack = true;
    _latency_us = latency_us;

    _refid = refid;
  }

  void setCRCMismatches(int crc_mismatches) {
    _crc_mismatches = crc_mismatches;
  }

  void setLogReading() { _log_reading = true; }
  void setReadErrors(int read_errors) { _read_errors = read_errors; }
  void setReadUS(uint64_t read_us) { _read_us = read_us; }
  void setWriteErrors(int write_errors) { _write_errors = write_errors; }
  void setWriteUS(uint64_t write_us) { _write_us = write_us; }

  static const char *typeString(ReadingType_t index) {
    static const char *type_strings[] = {"base",   "remote", "sensor", "boot",
                                         "switch", "text",   "pwm"};

    return type_strings[index];
  }

private:
  // reading metadata (id, measured time and type)
  string_t _id;
  time_t _mtime = time(nullptr); // time the reading was measureed

  // tracking info
  RefID_t _refid;
  bool _cmd_ack = false;
  string_t _cmd_err;
  uint32_t _latency_us = 0;

  bool _log_reading = false;

  uint64_t _read_us = 0;
  uint64_t _write_us = 0;
  int _crc_mismatches = 0;
  int _read_errors = 0;
  int _write_errors = 0;

  char *_json = nullptr;

  ReadingType_t _type = BASE;

  void commonJSON(JsonDocument &doc);
  virtual void populateJSON(JsonDocument &doc){};
};
} // namespace reading
} // namespace ruth

#endif // reading_h
