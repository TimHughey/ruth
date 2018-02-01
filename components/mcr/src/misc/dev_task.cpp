/*
 * timestamp_task.cpp
 *
 */

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>

#include <FreeRTOS.h>
#include <System.h>
#include <Task.h>
#include <WiFi.h>
#include <WiFiEventHandler.h>

#include <freertos/event_groups.h>

#include "celsius.hpp"
#include "cmd.hpp"
#include "dev_task.hpp"
#include "id.hpp"
#include "positions.hpp"

static char tTAG[] = "mcrDevTask";

mcrDevTask::mcrDevTask(EventGroupHandle_t evg, int bit)
    : Task(tTAG, (6 * 1024), 1) {
  ev_group = evg;
  wait_bit = bit;
}

mcrDevTask::~mcrDevTask() {}

void mcrDevTask::run(void *data) {
  std::random_device *r = new std::random_device();
  std::uniform_int_distribution<int> *uniform_dist =
      new std::uniform_int_distribution<int>(0, 255);
  // std::uniform_int_distribution<int> uniform_dist(0, 255);

  size_t prevHeap = System::getFreeHeapSize();
  size_t availHeap = 0;

  ESP_LOGD(tTAG, "started, waiting on event_group=%p for bits=0x%x",
           (void *)ev_group, wait_bit);
  xEventGroupWaitBits(ev_group, wait_bit, false, true, portMAX_DELAY);
  ESP_LOGD(tTAG, "event_group wait complete, entering task loop");

  _last_wake = xTaskGetTickCount();

  for (;;) {
    const size_t buff_len = 1024;
    char *buffer = new char[buff_len];

    bzero(buffer, buff_len);

    uint32_t states = (*uniform_dist)(*r);
    uint32_t mask = (*uniform_dist)(*r);

    mcrDevID_t dev("fake_dev");
    celsiusReading *reading = new celsiusReading(dev, time(nullptr), 31.3);

    positionsReading *positions =
        new positionsReading(dev, time(nullptr), states, 8);

    mcrCmd *cmd = new mcrCmd(dev, mask, states);

    availHeap = System::getFreeHeapSize();
    ESP_LOGI(tTAG, "after memory alloc  heap=%u delta=%d", availHeap,
             (int)(availHeap - prevHeap));
    prevHeap = availHeap;

    char *json = reading->json();
    ESP_LOGI(tTAG, "reading json (len=%u): %s", strlen(json), json);

    json = positions->json();
    ESP_LOGI(tTAG, "positions json (len=%u): %s", strlen(json), json);

    dev.debug(buffer, buff_len);
    ESP_LOGI(tTAG, "dev (sizeof=%u) debug: %s", sizeof(mcrDevID_t), buffer);

    bzero(buffer, buff_len);
    ESP_LOGI(tTAG, "cmd (sizeof=%u) debug: %s", sizeof(mcrCmd_t),
             cmd->debug().c_str());

    delete reading;
    delete positions;
    delete cmd;
    delete buffer;

    availHeap = System::getFreeHeapSize();
    ESP_LOGI(tTAG, "after memory dealloc heap=%u delta=%d", availHeap,
             (int)(availHeap - prevHeap));
    prevHeap = availHeap;

    vTaskDelayUntil(&_last_wake, _loop_frequency);
  }
}
