#include "cmds/factory.hpp"

namespace ruth {

static const char *TAG = "CmdFactory";

Cmd_t *CmdFactory::fromRaw(JsonDocument &doc, const char *raw) {
  Cmd_t *cmd = nullptr;
  elapsedMicros parse_elapsed;

  // deserialize the msgpack data
  DeserializationError err = deserializeMsgPack(doc, raw);

  // parsing complete, freeze the elapsed timer
  parse_elapsed.freeze();

  // did the deserailization succeed?
  // if so, manufacture the derived cmd
  if (err) {
    ESP_LOGW(TAG, "[%s] MSGPACK parse failure", err.c_str());
  } else {
    // deserialization success, manufacture the derived cmd
    cmd = manufacture(doc, parse_elapsed);
  }

  return cmd;
} // namespace ruth

Cmd_t *CmdFactory::manufacture(JsonDocument &doc,
                               elapsedMicros &parse_elapsed) {
  Cmd_t *cmd = nullptr;
  CmdType_t cmd_type = CmdType::unknown;

  auto cmd_str = doc["cmd"].as<string_t>();
  cmd_type = CmdTypeMap::fromString(cmd_str);

  switch (cmd_type) {
  case CmdType::unknown:
    ESP_LOGW(TAG, "unknown command [%s]", cmd_str.c_str());
    cmd = new Cmd(doc, parse_elapsed);
    break;

  case CmdType::none:
  case CmdType::heartbeat:
  case CmdType::timesync:
    cmd = new Cmd(doc, parse_elapsed);
    break;

  case CmdType::setswitch:
    cmd = new cmdSwitch(doc, parse_elapsed);
    break;

  case CmdType::setname:
    // cmd = new CmdNetwork(doc, parse_elapsed);
    break;

  case CmdType::otaHTTPS:
  case CmdType::restart:
    cmd = new CmdOTA(doc, parse_elapsed);
    break;

  case CmdType::enginesSuspend:
    break;

  case CmdType::pwm:
    cmd = new cmdPWM(doc, parse_elapsed);
    break;
  }

  return cmd;
}
} // namespace ruth
