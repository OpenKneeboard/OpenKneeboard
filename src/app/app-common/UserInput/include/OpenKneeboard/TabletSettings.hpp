// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/UserAction.hpp>

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

  bool mWarnIfOTDIPCUnusuable {false};

  std::unordered_map<std::string, Device> mDevices;

  bool operator==(const TabletSettings&) const noexcept = default;
};

NLOHMANN_JSON_SERIALIZE_ENUM(
  TabletOrientation,
  {
    {TabletOrientation::Normal, "Normal"},
    {TabletOrientation::RotateCW90, "RotateCW90"},
    {TabletOrientation::RotateCW180, "RotateCW180"},
    {TabletOrientation::RotateCW270, "RotateCCW270"},
  })
OPENKNEEBOARD_DECLARE_SPARSE_JSON(TabletSettings);

};// namespace OpenKneeboard
