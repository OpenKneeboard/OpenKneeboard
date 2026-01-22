// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/json.hpp>

#include <format>

namespace OpenKneeboard {

#define OPENKNEEBOARD_USER_ACTIONS \
  IT(CYCLE_ACTIVE_VIEW) \
  IT(DECREASE_BRIGHTNESS) \
  IT(DISABLE_TINT) \
  IT(ENABLE_TINT) \
  IT(HIDE) \
  IT(INCREASE_BRIGHTNESS) \
  IT(NEXT_BOOKMARK) \
  IT(NEXT_PAGE) \
  IT(NEXT_PROFILE) \
  IT(NEXT_TAB) \
  IT(PREVIOUS_BOOKMARK) \
  IT(PREVIOUS_PAGE) \
  IT(PREVIOUS_PROFILE) \
  IT(PREVIOUS_TAB) \
  IT(RECENTER_VR) \
  IT(REPAINT_NOW) \
  IT(SHOW) \
  IT(SWAP_FIRST_TWO_VIEWS) \
  IT(TOGGLE_BOOKMARK) \
  IT(TOGGLE_FORCE_ZOOM) \
  IT(TOGGLE_TINT) \
  IT(TOGGLE_VISIBILITY)

enum class UserAction {
#define IT(x) x,
  OPENKNEEBOARD_USER_ACTIONS
#undef IT
};

#define IT(ACTION) {UserAction::ACTION, #ACTION},
NLOHMANN_JSON_SERIALIZE_ENUM(UserAction, {OPENKNEEBOARD_USER_ACTIONS})
#undef IT

inline constexpr std::string to_string(UserAction action) {
  switch (action) {
#define IT(ACTION) \
  case UserAction::ACTION: \
    return #ACTION;
    OPENKNEEBOARD_USER_ACTIONS
#undef IT
    default:
      return std::format("Unknown UserAction: {}", std::to_underlying(action));
  }
}

}// namespace OpenKneeboard
