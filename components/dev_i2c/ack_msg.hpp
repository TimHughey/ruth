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

#ifndef i2c_ack_message_hpp
#define i2c_ack_message_hpp

#include <memory>

#include "message/out.hpp"

namespace i2c {

class Ack : public message::Out {
public:
  Ack(const char *refid);
  ~Ack() = default;

private:
  void assembleData(JsonObject &data);
};
} // namespace i2c
#endif
