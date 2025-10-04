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
#include <OpenKneeboard/ReloadTabAction.hpp>
#include <OpenKneeboard/TabNextPageAction.hpp>
#include <OpenKneeboard/TabPreviousPageAction.hpp>
#include <OpenKneeboard/ToggleBookmarkAction.hpp>
#include <OpenKneeboard/UserActionHandler.hpp>

namespace OpenKneeboard {

UserActionHandler::~UserActionHandler() = default;

std::unique_ptr<UserActionHandler> UserActionHandler::Create(
  KneeboardState* kneeboard,
  const std::shared_ptr<KneeboardView>& kneeboardView,
  const std::shared_ptr<TabView>& tab,
  UserAction action) {
  switch (action) {
    case UserAction::PREVIOUS_PAGE:
      return std::make_unique<TabPreviousPageAction>(kneeboard, tab);
    case UserAction::NEXT_PAGE:
      return std::make_unique<TabNextPageAction>(kneeboard, tab);
    case UserAction::TOGGLE_BOOKMARK:
      return std::make_unique<ToggleBookmarkAction>(
        kneeboard, kneeboardView, tab);
    default:
      return {nullptr};
  }
}

}// namespace OpenKneeboard
