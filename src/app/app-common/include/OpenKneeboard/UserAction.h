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

#include <OpenKneeboard/json.h>

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
  IT(RELOAD_CURRENT_TAB) \
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
      return std::format(
        "Unknown UserAction: {}",
        static_cast<std::underlying_type_t<UserAction>>(action));
  }
}

}// namespace OpenKneeboard
