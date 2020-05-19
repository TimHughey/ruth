
#include "cmds/types.hpp"

namespace ruth {

static const char *TAG = "CmdTypeMap";

static const std::map<string_t, CmdType> _cmd_map = {
    {string_t("unknown"), CmdType::unknown},
    {string_t("none"), CmdType::none},
    {string_t("set.switch"), CmdType::setswitch}};

static CmdTypeMap_t *__singleton;

CmdTypeMap::CmdTypeMap() {
  ESP_LOGD(TAG, "_cmd_set sizeof=%u", sizeof(_cmd_map));
}

// STATIC!
CmdTypeMap_t *CmdTypeMap::_instance_() {
  if (__singleton == nullptr) {
    __singleton = new CmdTypeMap();
  }

  return __singleton;
}

CmdType_t CmdTypeMap::find(const string_t &cmd) {
  auto search = _cmd_map.find(cmd);

  if (search != _cmd_map.end()) {
    return search->second;
  }

  ESP_LOGD(TAG, "unknown cmd=%s", cmd.c_str());

  return CmdType::unknown;
}
} // namespace ruth
