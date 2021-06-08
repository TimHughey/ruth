/*
    binder.cpp --  Abstraction for ESP32 SPIFFS
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

#include <cstdlib>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <esp_log.h>

#include <esp_system.h>
#include <freertos/FreeRTOS.h>

#include "core/binder.hpp"
#include "external/ArduinoJson.h"
#include "misc/datetime.hpp"

namespace ruth {
static Binder_t __singleton__;
static const char TAG[] = "Binder";

size_t Binder::copyToFilesystem() {
  StaticJsonDocument<_doc_capacity> doc_cp = _embed_doc;
  Raw mp_buff;

  doc_cp["meta"]["mtime"] = time(nullptr);
  auto mp_bytes = serializeMsgPack(doc_cp, mp_buff.data(), mp_buff.capacity());
  mp_buff.forceSize(mp_bytes);

  int fd = creat(_binder_file, S_IRUSR | S_IWUSR);

  ssize_t bytes = 0;
  if (fd >= 0) {
    bytes = write(fd, mp_buff.data(), mp_buff.size());
    close(fd);
  }

  return bytes;
}

DeserializationError Binder::deserialize(JsonDocument &doc, Raw &buff) const {
  return deserializeMsgPack(doc, buff.data(), buff.size());
}

void Binder::_init_() {
  esp_err_t esp_rc = ESP_OK;

  // register ruthfs fatfs
  esp_rc = esp_vfs_fat_spiflash_mount(_base_path, "ruthfs", &_mount_config,
                                      &_s_wl_handle);

  if (esp_rc == ESP_OK) {
    load();
    parse();
  } else {
    ESP_LOGE(TAG, "[%s] fatfs mount", esp_err_to_name(esp_rc));
    vTaskDelay(portMAX_DELAY);
  }
}

void Binder::load() {
  _embed_raw.assign(_embed_start_, _embed_end_);

  int fd = open(_binder_file, O_RDONLY);
  if (fd == -1) {
    _file_raw.clear();
  } else {
    auto bytes = read(fd, _file_raw.data(), _file_raw.capacity());
    if (bytes > 0) {
      _file_raw.forceSize(bytes);
    }

    close(fd);
  }
}

// int Binder::ls(const char *path) {
//   struct dirent *entry;
//   struct stat entry_stat;
//
//   const char *base_path = (path == nullptr) ? _base_path : path;
//
//   DIR *dir = opendir(base_path);
//
//   if (dir == nullptr) {
//     printf("\nbinder: failed to open directory \"%s\" for read", _base_path);
//     return 1;
//   }
//
//   while ((entry = readdir(dir)) != nullptr) {
//     TextBuffer<32> entry_path;
//
//     entry_path.printf("%s/%s", base_path, entry->d_name);
//     if (stat(entry_path.c_str(), &entry_stat) == 0) {
//       DateTime dt(entry_stat.st_mtime, "%b %e %H:%M");
//
//       const char type = (S_ISDIR(entry_stat.st_mode) ? 'd' : '-');
//       printf("%crw-r--r-- ruth ruth %6ld %s %s\n", type, entry_stat.st_size,
//              dt.c_str(), entry->d_name);
//     }
//   }
//
//   closedir(dir);
//
//   return 0;
// }

void Binder::parse() {
  DeserializationError embed_err = deserialize(_embed_doc, _embed_raw);
  DeserializationError file_err = deserialize(_file_doc, _file_raw);

  _versions[0] = (!embed_err) ? _embed_doc["meta"]["mtime"] : 0;
  _versions[1] = (!file_err) ? _file_doc["meta"]["mtime"] : 0;

  _root = (_versions[0] >= _versions[1]) ? _embed_doc.as<JsonObject>()
                                         : _file_doc.as<JsonObject>();

  _cli = _root["cli"];
  _lightdesk = _root["lightdesk"];
  _dmx = _lightdesk["dmx"];
  _meta = _root["meta"];
  _mqtt = _root["mqtt"];
  _ntp_servers = _root["ntp"];
  _ota = _root["ota"];
  _wifi = _root["wifi"];

  float used_percent =
      ((float)_root.memoryUsage() / (float)_doc_capacity) * 100.0;
  ESP_LOGI(TAG, "%s doc_used[%2.1f%%] (%d/%d)",
           DateTime(_meta["mtime"].as<uint32_t>()).c_str(), used_percent,
           _root.memoryUsage(), _doc_capacity);

  if (embed_err && file_err) {
    ESP_LOGW(TAG, "parse failed %s, bytes[%d]", embed_err.c_str(),
             _embed_raw.length());
    vTaskDelay(portMAX_DELAY);
  }
}

size_t Binder::pretty(PrettyJson &buff) {
  auto bytes = serializeJsonPretty(_root, buff.data(), buff.capacity());
  buff.forceSize(bytes);

  return bytes;
}

int Binder::rm(const char *file) {
  const char *x = (file == nullptr) ? _binder_file : file;

  return unlink(x);
}

int Binder::versions() {
  time_t versions[] = {_embed_doc["meta"]["mtime"], 0};

  int fd = open(_binder_file, O_RDONLY);
  if (fd >= 0) {
    Raw buff;
    StaticJsonDocument<_doc_capacity> doc_fs;

    auto bytes = read(fd, buff.data(), buff.capacity());

    close(fd);

    if (bytes > 0) {
      buff.forceSize(bytes);
      DeserializationError err = deserialize(doc_fs, buff);

      if (!err) {
        versions[1] = doc_fs["meta"]["mtime"];
      }
    }
  }

  const char *loc[] = {"<firmware>", _binder_file};
  bool latest[] = {versions[0] >= versions[1], versions[1] > versions[0]};
  for (int i = 0; i < (sizeof(versions) / sizeof(time_t)); i++) {
    DateTime ts(versions[i], "%b %e %H:%M:%S");

    printf("%s %s %s\n", (latest[i]) ? "latest" : "      ",
           (versions[i] > 0) ? ts.c_str() : " <unavailable> ", loc[i]);
  }

  return 0;
}

// STATIC
Binder_t *Binder::i() { return &__singleton__; }

} // namespace ruth
