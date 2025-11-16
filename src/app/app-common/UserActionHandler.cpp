// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
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
