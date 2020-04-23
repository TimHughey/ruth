
#include "cmds/pwm.hpp"
#include "cmds/queues.hpp"

namespace ruth {

cmdPWM::cmdPWM(JsonDocument &doc, elapsedMicros &e) : Cmd(doc, e, "device") {
  // json format of states command:
  // {"device":"pwm/ruth.xxx.pin:n",
  //   "duty":2048,
  //   "refid":"0fc4417c-f1bb-11e7-86bd-6cf049e7139f",
  //   "mtime":1515117138,
  //   "cmd":"pwm"}

  // overrides the default of internal name == external name
  translateExternalDeviceID("self");

  _duty = doc["duty"] | 0;
  _direction = doc["direction"] | 2;
  _step_num = doc["step_num"] | 0;
  _duty_cycle_num = doc["duty_cycle_num"] | 0;
  _duty_scale = doc["duty_scale"] | 0;

  _create_elapsed.freeze();
}

bool cmdPWM::process() {
  for (auto cmd_q : CmdQueues::all()) {
    auto *fresh_cmd = new cmdPWM(this);
    sendToQueue(cmd_q, fresh_cmd);
  }

  return true;
}

const unique_ptr<char[]> cmdPWM::debug() {
  const auto max_len = 128;
  unique_ptr<char[]> debug_str(new char[max_len + 1]);

  snprintf(debug_str.get(), max_len,
           "cmdPWM(%s duty(%d) dir(%s) step(%d) cycles(%d) scale(%d) %s)",
           _external_dev_id.c_str(), _duty, ((_direction == 0) ? "fwd" : "rev"),
           _step_num, _duty_cycle_num, _duty_scale, ((_ack) ? "ACK" : ""));

  return move(debug_str);
}
} // namespace ruth
