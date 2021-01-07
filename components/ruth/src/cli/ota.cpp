/*
    cli/ota.cpp - Ruth CLI Over-The-Air
    Copyright (C) 2020  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include <argtable3/argtable3.h>
#include <stdio.h>
#include <string.h>

#include "cli/ota.hpp"
#include "core/binder.hpp"

namespace ruth {

static struct arg_end *end;
static struct arg_file *fw_file;
static struct arg_str *host, *path;
static struct arg_lit *mark_valid;

static void *argtable[] = {
    host = arg_strn("h", "host", "<hostname>", 0, 1,
                    "https://<HOSTNAME>/<path>/<file>"),
    path = arg_strn("p", "path", "<path to file>", 0, 1,
                    "https://<hostname>/<PATH>/<file>"),
    fw_file = arg_filen("f", "file", "<filename>", 0, 1,
                        "https://<hostname>/<path>/<FILE>"),
    mark_valid =
        arg_litn(nullptr, "mark-valid", 0, 1, "mark ota valid, if needed"),
    end = arg_end(4),
};

int OtaCli::execute(int argc, char **argv) {
  Binder_t *binder = Binder::instance();

  mark_valid->count = 0;
  host->sval[0] = binder->otaHost();
  path->sval[0] = binder->otaPath();
  fw_file->filename[0] = binder->otaFile();

  int nerrors = arg_parse(argc, argv, argtable);

  if (nerrors > 0) {
    return 1;
  }

  if (mark_valid->count > 0) {
    OTA::markPartitionValid();
  } else {

    OtaUri_t uri;

    uri.printf("https://%s/%s/%s", host->sval[0], path->sval[0],
               fw_file->filename[0]);

    StaticJsonDocument<128> doc;
    JsonObject root = doc.to<JsonObject>();
    root["uri"] = uri.c_str();

    TextBuffer<128> msgpack;
    auto bytes = serializeMsgPack(doc, msgpack.data(), msgpack.capacity());
    msgpack.forceSize(bytes);

    OTA::queuePayload(msgpack.c_str());
  }

  return 0;
}

void OtaCli::registerArgTable() {
  static esp_console_cmd_t cmd = {};
  cmd.command = "ota";
  cmd.help = "OTA actions and maintenance";
  cmd.hint = NULL;
  cmd.func = &execute;
  cmd.argtable = argtable;

  esp_console_cmd_register(&cmd);
}

}; // namespace ruth
