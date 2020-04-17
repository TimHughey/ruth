#include "cmds/network.hpp"

namespace ruth {

// static const char *TAG = "CmdNetwork";
static const char *k_name = "name";

CmdNetwork::CmdNetwork(JsonDocument &doc, elapsedMicros &e) : Cmd{doc, e} {
  _name = doc[k_name] | "";
}

bool CmdNetwork::process() {
  if (forThisHost() && (_name.empty() == false)) {
    Net::setName(_name);
    return true;
  }

  return false;
}

const unique_ptr<char[]> CmdNetwork::debug() {
  const auto max_buf = 128;
  unique_ptr<char[]> debug_str(new char[max_buf]);

  snprintf(debug_str.get(), max_buf,
           "CmdNetwork(host(%s) name(%s)) parse(%lldus)", host().c_str(),
           _name.c_str(), (uint64_t)_parse_elapsed);

  return move(debug_str);
}
} // namespace ruth
