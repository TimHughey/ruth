#include "cmds/base.hpp"

using std::move;
using std::string;
using std::unique_ptr;

namespace ruth {

static const char *k_mtime = "mtime";
static const char *k_cmd = "cmd";

// Cmd::Cmd(JsonDocument &doc, elapsedMicros &e) : _parse_elapsed(e) {
//   populate(doc);
// }

// Cmd::Cmd(CmdType_t type) {
//   _mtime = time(nullptr);
//   _type = type;
// }

// BASE CLASS copy from a pointer to a Cmd_t
Cmd::Cmd(const Cmd_t *cmd) {
  // PRIVATE MEMBERS
  _mtime = cmd->_mtime;
  _type = cmd->_type;
  _host = cmd->_host;

  // PROTECTED MEMBERS
  _external_dev_id = cmd->_external_dev_id;
  _internal_dev_id = cmd->_internal_dev_id;
  _refid = cmd->_refid;
  _ack = cmd->_ack;
  _parse_elapsed = cmd->_parse_elapsed;
  _create_elapsed = cmd->_create_elapsed;
  _latency_us = cmd->_latency_us;
}

Cmd::Cmd(JsonDocument &doc, elapsedMicros &e) : _parse_elapsed(e) {
  populate(doc);
}

Cmd::Cmd(JsonDocument &doc, elapsedMicros &e, const char *dev_name_key)
    : _parse_elapsed(e) {
  populate(doc, dev_name_key);
}

bool Cmd::forThisHost() const {
  const string_t &mac_addr = Net::macAddress();

  auto found = _host.find(mac_addr);

  // didn't match, check for '<any>'
  if (found == string::npos) {
    found = _host.find("<any>");
  }

  return (found == string::npos) ? false : true;
}

bool Cmd::matchExternalDevID() {
  const string_t &name = Net::getName();
  auto match_pos = _external_dev_id.find(name);

  return (match_pos == string::npos) ? false : true;
}

bool Cmd::matchPrefix(const char *prefix) {
  return ((_external_dev_id.substr(0, strlen(prefix))) == prefix);
}

// populates the cmd from the JsonDocument for non-specific device cmds
void Cmd::populate(JsonDocument &doc) {
  _mtime = doc[k_mtime] | time(nullptr);
  _type = CmdTypeMap::fromString(doc[k_cmd] | "unknown");
  _ack = doc["ack"] | false;
  _refid = doc["refid"] | "";
  _host = doc["host"] | "<any>";
}

void Cmd::populate(JsonDocument &doc, const char *dev_name_key) {
  populate(doc); // call populate for the non-specific dev cmd

  const JsonVariant external_device = doc[dev_name_key];

  if (external_device.isNull() == false) {
    _external_dev_id = external_device.as<string_t>();
  }

  populateInternalDevice(doc);
}

// unless overridden by a derived class set the internal device name
// equal to the external device name
void Cmd::populateInternalDevice(JsonDocument &doc) {
  _internal_dev_id = _external_dev_id; // default to external name
}

bool Cmd::sendToQueue(cmdQueue_t &cmd_q, Cmd_t *cmd) {
  auto rc = false;
  auto q_rc = pdTRUE;

  if (matchPrefix(cmd_q.prefix)) {
    // make a fresh copy of the cmd before pushing to the queue to ensure:
    //   a. each queue receives it's own copy
    //   b. we're certain each cmd is in a clean state

    // pop the oldest cmd (at the front) to make space when the queue is full
    if (uxQueueSpacesAvailable(cmd_q.q) == 0) {
      Cmd_t *old_cmd = nullptr;

      q_rc = xQueueReceive(cmd_q.q, &old_cmd, pdMS_TO_TICKS(10));

      if ((q_rc == pdTRUE) && (old_cmd != nullptr)) {
        textReading_t *rlog(new textReading_t);
        textReading_ptr_t rlog_ptr(rlog);
        rlog->printf("[%s] queue FULL, removing oldest cmd (%s)", cmd_q.id,
                     old_cmd->externalDevID().c_str());
        rlog->publish();
        delete old_cmd;
      }
    }

    if (q_rc == pdTRUE) {
      q_rc = xQueueSendToBack(cmd_q.q, (void *)&cmd, pdMS_TO_TICKS(10));

      if (q_rc == pdTRUE) {
        rc = true;
      } else {
        textReading_t *rlog(new textReading_t);
        textReading_ptr_t rlog_ptr(rlog);
        rlog->printf("[%s] queue FAILURE for %s", cmd_q.id,
                     _external_dev_id.c_str());
        rlog->publish();

        delete cmd; // delete the cmd passed in since it was never used
      }
    }

  } else {
    delete cmd; // delete the cmd passed since it was never used
  }

  return rc;
}

void Cmd::translateExternalDeviceID(const char *replacement) {
  const string_t &name = Net::getName();

  // update the internal dev ID (originally external ID)
  auto pos = _internal_dev_id.find(name);

  if (pos == string::npos) {
    // didn't find the name of this host, not for us
  } else {
    _internal_dev_id.replace(pos, name.length(), replacement);
  }
}

const unique_ptr<char[]> Cmd::debug() {

  const auto max_buf = 256;
  unique_ptr<char[]> debug_str(new char[max_buf]);

  snprintf(debug_str.get(), max_buf,
           "Cmd(host(%s) latency_us(%lldus) parse(%lldus) create(%lldus)",
           _host.c_str(), (uint64_t)latency_us(), (uint64_t)_parse_elapsed,
           (uint64_t)_create_elapsed);

  return move(debug_str);
}
} // namespace ruth
