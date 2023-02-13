#pragma once

#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/ringbuf.h>

typedef struct {
  rmt_channel_handle_t tx_channel;
  rmt_channel_handle_t rx_channel;
  RingbufHandle_t rb;
  int gpio;

  OneWireBus bus;
} owb_rmt_driver_info;

OneWireBus *owb_rmt_initialize(owb_rmt_driver_info *info, uint8_t gpio_num);
