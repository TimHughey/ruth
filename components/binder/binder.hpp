/*
    binder.hpp -- Abstraction for ESP32 SPIFFS
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

#ifndef _ruth_binder_hpp
#define _ruth_binder_hpp

#include <memory>

#include <esp_vfs.h>
#include <esp_vfs_fat.h>

#include "ArduinoJson.h"
#include "misc/textbuffer.hpp"

typedef class Binder Binder_t;

class Binder {

public:
  typedef TextBuffer<512> Raw;
  typedef TextBuffer<768> PrettyJson;

public:
  Binder(){}; // SINGLETON
  static void init();
  static Binder_t *instance() { return i(); }

  // cli interface
  const char *basePath() const { return _base_path; }
  size_t copyToFilesystem();

  size_t pretty(PrettyJson &buff);
  int print();
  int rm(const char *path = nullptr);
  int versions();

  static const char *env();
  static const JsonObject mqtt();
  static const JsonArray ntp();
  static const JsonObject wifi();

private:
  void _init_();
  static Binder *i();

  DeserializationError deserialize(JsonDocument &doc, Raw &buff) const;
  void load();
  void parse();

private:
  const esp_vfs_fat_mount_config_t _mount_config = {
      .format_if_mount_failed = true, .max_files = 4, .allocation_unit_size = CONFIG_WL_SECTOR_SIZE};

  wl_handle_t _s_wl_handle = WL_INVALID_HANDLE;
  const char *_base_path = "/r";

  static const size_t _doc_capacity = 512;
  static const uint8_t _embed_start_[] asm("_binary_binder_0_mp_start");
  static const uint8_t _embed_end_[] asm("_binary_binder_0_mp_end");
  static const size_t _embed_length_ asm("binder_0_mp_length");

  const char *_binder_file = "/r/binder_0.mp";

  Raw _embed_raw;
  Raw _file_raw;

  StaticJsonDocument<_doc_capacity> _embed_doc;
  StaticJsonDocument<_doc_capacity> _file_doc;
  JsonObject _root;
  JsonObject _meta;

  time_t _versions[2] = {0, 0};
};

#endif
