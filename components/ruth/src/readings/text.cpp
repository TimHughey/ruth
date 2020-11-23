/*
    simple_text.cpp - Simple Text (aka Log) Reading
    Copyright (C) 2019  Tim Hughey

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

#include <sys/time.h>
#include <time.h>

#include "protocols/mqtt.hpp"
#include "readings/text.hpp"

namespace ruth {
Text::Text() : Reading(TEXT) {}
Text::Text(const char *text) : Reading(TEXT) {
  strncpy(_actual, text, maxLength());
}

Text::~Text() {}

void Text::consoleInfo(const char *tag) {
  if (_actual[0])
    ESP_LOGI(tag, "%s", _actual);
}

void Text::consoleErr(const char *tag) {
  if (_actual[0])
    ESP_LOGE(tag, "%s", _actual);
}

void Text::consoleWarn(const char *tag) {
  if (_actual[0])
    ESP_LOGW(tag, "%s", _actual);
}

void Text::populateMessage(JsonDocument &doc) {
  doc["text"] = _actual;
  doc["log"] = true;
}

void Text::printf(const char *format, ...) {
  va_list arglist;
  size_t bytes;

  va_start(arglist, format);
  bytes = vsnprintf(_append_text, availableBytes(), format, arglist);
  va_end(arglist);

  use(bytes);
}

void Text::printf(struct tm *timeinfo, const char *format, ...) {
  va_list arglist;
  size_t bytes;

  // print the formatted string to the buffer and use the bytes
  va_start(arglist, format);
  bytes = vsnprintf(_actual, availableBytes(), format, arglist);
  va_end(arglist);

  use(bytes);

  // alloc memory for the time string then append it to the buffer
  const size_t time_str_max_len = 40;
  std::unique_ptr<char[]> time_str(new char[time_str_max_len + 1]);

  strftime(time_str.get(), time_str_max_len, "%F %T", timeinfo);

  bytes = snprintf(append(), availableBytes(), " time(%s)", time_str.get());

  use(bytes);
}

void Text::publish() {
  if (_actual[0]) {
    MQTT::publish(this);
  }
}

void Text::rlog(const char *format, ...) {
  va_list arglist;
  Text_ptr_t log_ptr(new Text_t);
  Text_t *log = log_ptr.get();

  // protect against memory allocation failures
  if (log) {
    size_t bytes;

    va_start(arglist, format);
    bytes =
        vsnprintf(log->_append_text, log->availableBytes(), format, arglist);
    va_end(arglist);

    log->use(bytes);
    log->publish();
  }
}

void Text::rlog(struct tm *timeinfo, const char *format, ...) {
  va_list arglist;
  size_t bytes;

  Text_ptr_t log_ptr(new Text_t);
  Text_t *log = log_ptr.get();

  if (log) {
    // print the formatted string to the buffer and use the bytes
    va_start(arglist, format);
    bytes = vsnprintf(log->_actual, log->availableBytes(), format, arglist);
    va_end(arglist);

    log->use(bytes);

    // alloc memory for the time string then append it to the buffer
    const size_t time_str_max_len = 40;
    std::unique_ptr<char[]> time_str(new char[time_str_max_len + 1]);

    strftime(time_str.get(), time_str_max_len, "%F %T", timeinfo);

    bytes = snprintf(log->append(), log->availableBytes(), " time(%s)",
                     time_str.get());

    log->use(bytes);
    log->publish();
  }
}

} // namespace ruth
