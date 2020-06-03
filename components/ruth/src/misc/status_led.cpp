
#include "misc/status_led.hpp"

namespace ruth {
static const char *TAG = "StatusLED";
static StatusLED_t *__singleton__ = nullptr;

StatusLED::StatusLED() {
  auto timer_rc = ESP_FAIL;
  auto config_rc = ESP_FAIL;
  auto fade_func_rc = ESP_FAIL;

  timer_rc = ledc_timer_config(&ledc_timer_);

  if (timer_rc == ESP_OK) {
    config_rc = ledc_channel_config(&ledc_channel_);

    if (config_rc == ESP_OK) {
      fade_func_rc = ledc_fade_func_install(ESP_INTR_FLAG_LEVEL1);

      if (fade_func_rc == ESP_OK) {
        ledc_set_duty_and_update(ledc_channel_.speed_mode,
                                 ledc_channel_.channel, duty_, 0);
      }
    }
  }

  ESP_LOGI(TAG, "timer: [%s] config: [%s] fade_func: [%s]",
           esp_err_to_name(timer_rc), esp_err_to_name(config_rc),
           esp_err_to_name(fade_func_rc));
}

void StatusLED::_bright_() {
  duty_ = duty_max_ / 2;
  activate_duty();
}

void StatusLED::_brighter_() {
  duty_ += 1024;

  if (duty_ > duty_max_) {
    duty_ = duty_max_;
  }

  activate_duty();
}

void StatusLED::_dim_() {
  duty_ = duty_max_ / 90;
  activate_duty();
}

void StatusLED::_dimmer_() {
  if (duty_ > 1025) {
    duty_ -= 1025;
  }

  activate_duty();
}

void StatusLED::_off_() {
  duty_ = 0;
  activate_duty();
}

void StatusLED::_percent_(float p) {
  uint32_t val = uint32_t(duty_max_ * p);

  duty_ = val;
  activate_duty();
}

void StatusLED::activate_duty() {
  ledc_set_duty_and_update(ledc_channel_.speed_mode, ledc_channel_.channel,
                           duty_, 0);
}

void StatusLED::_duty_(uint32_t new_duty) {

  if (new_duty <= duty_max_) {
    duty_ = new_duty;

    activate_duty();
  }
}

// STATIC
StatusLED_t *StatusLED::_instance_() {
  if (__singleton__ == nullptr) {
    __singleton__ = new StatusLED();
  }

  return __singleton__;
}
} // namespace ruth
