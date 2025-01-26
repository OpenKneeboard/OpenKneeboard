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
#include <OpenKneeboard/ClearUserInputAction.hpp>
#include <OpenKneeboard/CreateTabActions.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/NextTabAction.hpp>
#include <OpenKneeboard/PreviousTabAction.hpp>
#include <OpenKneeboard/ReloadTabAction.hpp>
#include <OpenKneeboard/SwitchProfileFlyout.hpp>
#include <OpenKneeboard/SwitchTabFlyout.hpp>
#include <OpenKneeboard/TabDeveloperToolsAction.hpp>
#include <OpenKneeboard/TabFirstPageAction.hpp>
#include <OpenKneeboard/TabNavigationAction.hpp>
#include <OpenKneeboard/TabNextPageAction.hpp>
#include <OpenKneeboard/TabPreviousPageAction.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/ToggleBookmarkAction.hpp>
#include <OpenKneeboard/ToolbarAction.hpp>
#include <OpenKneeboard/ToolbarFlyout.hpp>
#include <OpenKneeboard/ToolbarSeparator.hpp>

#include <ranges>

namespace OpenKneeboard {

namespace {
using ItemPtr = std::shared_ptr<IToolbarItem>;
using Items = std::vector<ItemPtr>;
}// namespace

static ItemPtr CreateClearNotesItem(
  KneeboardState* kbs,
  const std::shared_ptr<KneeboardView>&,
  const std::shared_ptr<TabView>& tabView) {
  return std::make_shared<ToolbarFlyout>(
    "\ued60",// StrokeErase
    "Clear notes",
    Items {
      std::make_shared<ClearUserInputAction>(kbs, tabView, CurrentPage),
      std::make_shared<ClearUserInputAction>(kbs, tabView, AllPages),
      std::make_shared<ClearUserInputAction>(kbs, AllTabs),
    });
}

static ItemPtr CreateReloadItem(
  KneeboardState* kbs,
  const std::shared_ptr<KneeboardView>&,
  const std::shared_ptr<TabView>& tabView) {
  return std::make_shared<ToolbarFlyout>(
    "\ue72c",// Refresh
    "Reload",
    Items {
      std::make_shared<ReloadTabAction>(kbs, tabView),
      std::make_shared<ReloadTabAction>(kbs, AllTabs),
    });
}

InGameActions InGameActions::Create(
  KneeboardState* kneeboardState,
  const std::shared_ptr<KneeboardView>& kneeboardView,
  const std::shared_ptr<TabView>& tabView) {
  return {
      .mLeft = {
    std::make_shared<TabNavigationAction>(tabView),
    std::make_shared<TabFirstPageAction>(tabView),
    std::make_shared<TabPreviousPageAction>(kneeboardState, tabView),
    std::make_shared<TabNextPageAction>(kneeboardState, tabView),
      },
      .mRight = {
    std::make_shared<ToolbarFlyout>(
      "\ue712",
      _("More"),
      Items {
        std::make_shared<SwitchProfileFlyout>(kneeboardState),
        std::make_shared<SwitchTabFlyout>(kneeboardState, kneeboardView),
        std::make_shared<ToolbarSeparator>(),
        CreateClearNotesItem(kneeboardState, kneeboardView, tabView),
        CreateReloadItem(kneeboardState, kneeboardView, tabView),
      }),
    std::make_shared<ToggleBookmarkAction>(kneeboardState, kneeboardView, tabView),
    std::make_shared<PreviousTabAction>(kneeboardState, kneeboardView),
    std::make_shared<NextTabAction>(kneeboardState, kneeboardView),
      },
  };
}

InAppActions InAppActions::Create(
  KneeboardState* kneeboardState,
  const std::shared_ptr<KneeboardView>& kneeboardView,
  const std::shared_ptr<TabView>& tabView) {
  return {
    .mPrimary = {
    std::make_shared<TabNavigationAction>(tabView),
    std::make_shared<TabFirstPageAction>(tabView),
    std::make_shared<TabPreviousPageAction>(kneeboardState, tabView),
    std::make_shared<TabNextPageAction>(kneeboardState, tabView),
    std::make_shared<ToggleBookmarkAction>(kneeboardState, kneeboardView, tabView),
    },
    .mSecondary = {
      CreateClearNotesItem(kneeboardState, kneeboardView, tabView),
      CreateReloadItem(kneeboardState, kneeboardView, tabView),
      std::make_shared<ToolbarSeparator>(),
      std::make_shared<TabDeveloperToolsAction>(kneeboardState, kneeboardView->GetRuntimeID(), tabView),
    },
  };
}

}// namespace OpenKneeboard
