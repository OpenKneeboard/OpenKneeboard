/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#pragma once

#include <nlohmann/json_fwd.hpp>

namespace OpenKneeboard {
template <class T>
void to_json_with_default(nlohmann::json& j, const T& parent_v, const T& v);
}// namespace OpenKneeboard

#define OPENKNEEBOARD_DECLARE_SPARSE_JSON(T) \
  void from_json(const nlohmann::json& nlohmann_json_j, T& nlohmann_json_v); \
  template <> \
  void to_json_with_default<T>( \
    nlohmann::json & nlohmann_json_j, \
    const T& nlohmann_json_default_object, \
    const T& nlohmann_json_v); \
  void to_json(nlohmann::json& nlohmann_json_j, const T& nlohmann_json_v);

#define OPENKNEEBOARD_DECLARE_JSON(T) \
  void from_json(const nlohmann::json& nlohmann_json_j, T& nlohmann_json_v); \
  void to_json(nlohmann::json& nlohmann_json_j, const T& nlohmann_json_v);
