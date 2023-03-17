// Ruth
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "desk_msg/kv.hpp"
#include "ru_base/types.hpp"

#include <algorithm>
#include <variant>
#include <vector>

namespace ruth {
namespace desk {

class kv_store {
private:
  using val_t = std::variant<uint16_t, uint32_t, float, bool, int64_t, string>;

  struct key_val_entry {
    string key;
    val_t val;
  };

public:
  kv_store() = default;
  ~kv_store() noexcept {} // delete default copy construct / assign

  kv_store(kv_store &&) = default;
  kv_store &operator=(kv_store &&) = default;

  inline void add(kv_store &&add_kvs) noexcept {
    std::for_each(add_kvs.key_vals.begin(), add_kvs.key_vals.end(),
                  [this](auto &entry) { key_vals.emplace_back(std::move(entry)); });
  }
  inline void add(auto key, auto &&val) noexcept { key_vals.emplace_back(key_val_entry{key, val}); }

  void add_heap_info() noexcept;

  inline void clear() noexcept { key_vals.clear(); }

  inline void populate_doc(auto &doc) noexcept {

    for (const auto &entry : key_vals) {
      // visit each entry value and add them to the document at the entry key
      std::visit([&](auto &&_val) { doc[entry.key] = _val; }, entry.val);
    }
  }

private:
  std::vector<key_val_entry> key_vals;

public:
  static constexpr csv module_id{"desk.msg.kv_store"};
};

} // namespace desk
} // namespace ruth
