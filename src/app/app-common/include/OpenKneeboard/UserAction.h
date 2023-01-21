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

#include <nlohmann/json.hpp>

namespace OpenKneeboard {

#define OPENKNEEBOARD_USER_ACTIONS \
  IT(PREVIOUS_BOOKMARK) \
  IT(NEXT_BOOKMARK) \
  IT(TOGGLE_BOOKMARK) \
  IT(PREVIOUS_TAB) \
  IT(NEXT_TAB) \
  IT(PREVIOUS_PAGE) \
  IT(NEXT_PAGE) \
  IT(PREVIOUS_PROFILE) \
  IT(NEXT_PROFILE) \
  IT(TOGGLE_VISIBILITY) \
  IT(TOGGLE_FORCE_ZOOM) \
  IT(SWITCH_KNEEBOARDS) \
  IT(RECENTER_VR) \
  IT(HIDE) \
  IT(SHOW)

enum class UserAction {
#define IT(x) x,
  OPENKNEEBOARD_USER_ACTIONS
#undef IT
};

#define IT(ACTION) {UserAction::ACTION, #ACTION},
NLOHMANN_JSON_SERIALIZE_ENUM(UserAction, {OPENKNEEBOARD_USER_ACTIONS})
#undef IT

}// namespace OpenKneeboard
