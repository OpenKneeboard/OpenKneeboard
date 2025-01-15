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

#include <OpenKneeboard/UserAction.hpp>
#include <OpenKneeboard/WintabMode.hpp>

#include <OpenKneeboard/json_fwd.hpp>

#include <cinttypes>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace OpenKneeboard {

enum class TabletOrientation {
  Normal,
  RotateCW90,
  RotateCW180,
  RotateCW270,
};

struct TabletSettings final {
  struct ButtonBinding final {
    std::unordered_set<uint64_t> mButtons;
    UserAction mAction;

    bool operator==(const ButtonBinding&) const noexcept = default;
  };
  struct Device {
    std::string mID;
    std::string mName;
    std::vector<ButtonBinding> mExpressKeyBindings;
    TabletOrientation mOrientation {TabletOrientation::RotateCW90};

    bool operator==(const Device&) const noexcept = default;
  };

  WintabMode mWintab {WintabMode::Disabled};
  bool mOTDIPC {true};
  bool mWarnIfOTDIPCUnusuable {false};

  std::unordered_map<std::string, Device> mDevices;

  bool operator==(const TabletSettings&) const noexcept = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(TabletSettings);

};// namespace OpenKneeboard
