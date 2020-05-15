#include <string.h>

#include "cmds/queues.hpp"

namespace ruth {

static const char *TAG = "CmdQueues";
static CmdQueues_t *__singleton = nullptr;

CmdQueues *CmdQueues::_instance_() {
  if (__singleton == nullptr) {
    __singleton = new CmdQueues();
  }

  return __singleton;
}

void CmdQueues::registerQ(cmdQueue_t &cmd_q) {
  ESP_LOGI(TAG, "registering cmd_q id=%s prefix=%s q=%p", cmd_q.id,
           cmd_q.prefix, (void *)cmd_q.q);

  _instance_()->add(cmd_q);
}

const unique_ptr<char[]> CmdQueues::debug() {
  unique_ptr<char[]> debug_str(new char[strlen(TAG) + 1]);

  strcpy(debug_str.get(), TAG);

  return move(debug_str);
}
} // namespace ruth
