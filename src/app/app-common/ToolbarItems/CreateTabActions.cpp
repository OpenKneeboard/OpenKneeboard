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
#include <OpenKneeboard/ClearUserInputAction.h>
#include <OpenKneeboard/CreateTabActions.h>
#include <OpenKneeboard/NextTabAction.h>
#include <OpenKneeboard/PreviousTabAction.h>
#include <OpenKneeboard/ReloadTabAction.h>
#include <OpenKneeboard/SetTabFlyout.h>
#include <OpenKneeboard/TabFirstPageAction.h>
#include <OpenKneeboard/TabNavigationAction.h>
#include <OpenKneeboard/TabNextPageAction.h>
#include <OpenKneeboard/TabPreviousPageAction.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/ToolbarAction.h>
#include <OpenKneeboard/ToolbarFlyout.h>
#include <OpenKneeboard/ToolbarSeparator.h>

namespace OpenKneeboard {

namespace {
using ItemPtr = std::shared_ptr<IToolbarItem>;
using Items = std::vector<ItemPtr>;
}// namespace

static ItemPtr CreateClearNotesItem(
  KneeboardState* kbs,
  const std::shared_ptr<IKneeboardView>&,
  const std::shared_ptr<ITabView>& tabView) {
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
  const std::shared_ptr<IKneeboardView>&,
  const std::shared_ptr<ITabView>& tabView) {
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
  const std::shared_ptr<IKneeboardView>& kneeboardView,
  const std::shared_ptr<ITabView>& tabView) {
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
        std::make_shared<SetTabFlyout>(kneeboardState, kneeboardView),
        std::make_shared<ToolbarSeparator>(),
        CreateClearNotesItem(kneeboardState, kneeboardView, tabView),
        CreateReloadItem(kneeboardState, kneeboardView, tabView),
      }),
    std::make_shared<PreviousTabAction>(kneeboardState, kneeboardView),
    std::make_shared<NextTabAction>(kneeboardState, kneeboardView),
      },
  };
}

InAppActions InAppActions::Create(
  KneeboardState* kneeboardState,
  const std::shared_ptr<IKneeboardView>& kneeboardView,
  const std::shared_ptr<ITabView>& tabView) {
  return {
    .mPrimary = {
    std::make_shared<TabNavigationAction>(tabView),
    std::make_shared<TabFirstPageAction>(tabView),
    std::make_shared<TabPreviousPageAction>(kneeboardState, tabView),
    std::make_shared<TabNextPageAction>(kneeboardState, tabView),
    },
    .mSecondary = {
      CreateClearNotesItem(kneeboardState, kneeboardView, tabView),
      CreateReloadItem(kneeboardState, kneeboardView, tabView),
    },
  };
}

}// namespace OpenKneeboard
