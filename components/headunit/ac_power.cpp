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

namespace ruth {

constexpr gpio_num_t pin = GPIO_NUM_21;

AcPower::AcPower(csv id) : HeadUnit(id) {
  gpio_config_t pins_cfg;

  pins_cfg.pin_bit_mask = GPIO_SEL_21;
  pins_cfg.mode = GPIO_MODE_OUTPUT;
  pins_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  pins_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  pins_cfg.intr_type = GPIO_INTR_DISABLE;

  gpio_config(&pins_cfg);
  gpio_set_level(pin, 0);
}

AcPower::~AcPower() { gpio_set_level(pin, 0); }

IRAM_ATTR bool AcPower::status() {
  auto pin_level = gpio_get_level(pin);
  return (pin_level > 0) ? true : false;
}

IRAM_ATTR bool AcPower::setLevel(bool level) {
  auto rc = false;
  bool pin_level = level ? 1 : 0;
  auto esp_rc = gpio_set_level(pin, pin_level);

  if (esp_rc == ESP_OK) {
    rc = true;
  }

  return rc;
}

} // namespace ruth
