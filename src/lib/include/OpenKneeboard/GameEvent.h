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

#include <OpenKneeboard/utf8.h>

#include <cstddef>
#include <string>
#include <vector>

namespace OpenKneeboard {
struct GameEvent final {
  utf8_string name;
  utf8_string value;

  operator bool() const;

  static GameEvent Unserialize(const std::vector<std::byte>& payload);
  std::vector<std::byte> Serialize() const;
  void Send() const;

  static const char* GetMailslotPath();

  static constexpr char EVT_REMOTE_USER_ACTION[]
    = "com.fredemmott.openkneeboard/RemoteUserAction";

  static constexpr char EVT_SET_INPUT_FOCUS[]
    = "com.fredemmott.openkneeboard/SetInputFocus";
};
}// namespace OpenKneeboard
