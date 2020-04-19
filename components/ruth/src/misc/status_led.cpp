
#include "misc/status_led.hpp"

namespace ruth {
static const char *TAG = "statusLED";
static statusLED_t *__singleton__ = nullptr;

statusLED::statusLED() {
  esp_err_t timer_rc = ESP_FAIL;
  esp_err_t config_rc = ESP_FAIL;
  esp_err_t fade_func_rc = ESP_FAIL;

  timer_rc = ledc_timer_config(&ledc_timer_);

  if (timer_rc == ESP_OK) {
    config_rc = ledc_channel_config(&ledc_channel_);

    if (config_rc == ESP_OK) {
      fade_func_rc = ledc_fade_func_install(ESP_INTR_FLAG_LOWMED);
    }
  }

  ESP_LOGI(TAG, "timer: [%s] config: [%s] fade_func: [%s]",
           esp_err_to_name(timer_rc), esp_err_to_name(config_rc),
           esp_err_to_name(fade_func_rc));
}

void statusLED::bright() {
  duty_ = 4095;
  activate_duty();
}

void statusLED::brighter() {
  duty_ += 512;

  if (duty_ > 4095) {
    duty_ = 4095;
  }

  activate_duty();
}

void statusLED::dim() {
  duty_ = 128;
  activate_duty();
}

void statusLED::dimmer() {
  duty_ -= 0;

  if (duty_ < 128) {
    duty_ = 128;
  }

  activate_duty();
}

void statusLED::off() {
  duty_ = 0;
  activate_duty();
}

void statusLED::activate_duty() {
  ledc_set_duty_and_update(ledc_channel_.speed_mode, ledc_channel_.channel,
                           duty_, 0);
}

// STATIC
void statusLED::duty(uint32_t new_duty) {

  if ((new_duty > 0) && (new_duty < 4096)) {
    instance()->duty_ = new_duty;

    instance()->activate_duty();
  }
}

// STATIC
statusLED_t *statusLED::instance() {
  if (__singleton__ == nullptr) {
    __singleton__ = new statusLED();
  }

  return __singleton__;
}
} // namespace ruth
