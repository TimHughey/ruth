/*
  Ruth
  (C)opyright 2021  Tim Hughey

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

#ifndef ds_states_message_hpp
#define ds_states_message_hpp

#include <memory>

#include "message/out.hpp"

namespace i2c {

class States : public message::Out {
public:
  enum Status : uint8_t { OK = 0, ERROR = 1 };

public:
  States(const char *device_name);
  ~States() = default;

  void addPin(uint8_t pin_num, const char *status);
  inline void finalize() { _read_us = now() - _start_at; }
  void setError() { _status = ERROR; }

private:
  void assembleData(JsonObject &data);
  static uint64_t now();

private:
  uint64_t _start_at;
  uint64_t _read_us;
  Status _status = OK;
};
} // namespace i2c
#endif
