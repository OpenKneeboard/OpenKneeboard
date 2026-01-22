// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/UserAction.hpp>

#include <OpenKneeboard/json_fwd.hpp>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct IDirectInput8W;

namespace OpenKneeboard {

struct DirectInputSettings {
  struct ButtonBinding {
    std::unordered_set<uint64_t> mButtons;
    UserAction mAction;
    bool operator==(const ButtonBinding&) const noexcept = default;
  };
  struct Device {
    std::string mID;
    std::string mName;
    std::string mKind;
    std::vector<ButtonBinding> mButtonBindings;
    bool operator==(const Device&) const noexcept = default;
  };

  bool mEnableMouseButtonBindings {false};
  std::unordered_map<std::string, Device> mDevices;

  bool operator==(const DirectInputSettings&) const noexcept = default;
};
OPENKNEEBOARD_DECLARE_SPARSE_JSON(DirectInputSettings);
}// namespace OpenKneeboard
