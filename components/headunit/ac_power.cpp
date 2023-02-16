/*
    lightdesk/ac_power.hpp - Ruth LightDesk AC Power Switch
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

#include "headunit/ac_power.hpp"
#include "headunit/headunit.hpp"
#include "ru_base/types.hpp"

#include <driver/gpio.h>
#include <esp_attr.h>

namespace ruth {

DRAM_ATTR static constexpr gpio_num_t pin = GPIO_NUM_21;
DRAM_ATTR static constexpr gpio_config_t cfg{.pin_bit_mask = 1ULL << pin,
                                             .mode = GPIO_MODE_OUTPUT,
                                             .pull_up_en = GPIO_PULLUP_DISABLE,
                                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                             .intr_type = GPIO_INTR_DISABLE};

AcPower::AcPower(csv id) noexcept : HeadUnit(id) {
  gpio_config(&cfg);
  gpio_set_level(pin, 0);
}

AcPower::~AcPower() noexcept { gpio_set_level(pin, 0); }

bool IRAM_ATTR AcPower::status() noexcept { return (gpio_get_level(pin) > 0) ? true : false; }

bool IRAM_ATTR AcPower::setLevel(bool level) noexcept {
  if (ESP_OK == gpio_set_level(pin, level ? 1 : 0)) return true;

  return false;
}

} // namespace ruth
