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
#include <OpenKneeboard/TabFirstPageAction.h>
#include <OpenKneeboard/TabNavigationAction.h>
#include <OpenKneeboard/TabNextPageAction.h>
#include <OpenKneeboard/TabPreviousPageAction.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/ToolbarAction.h>
#include <OpenKneeboard/ToolbarFlyout.h>

namespace OpenKneeboard {

static std::vector<std::shared_ptr<IToolbarItem>> CreateDropDownItems(
  KneeboardState* kbs,
  const std::shared_ptr<IKneeboardView>& kneeboardView,
  const std::shared_ptr<ITabView>& tabView) {
  auto rootTab = tabView->GetRootTab();
  return {
    std::make_shared<ToolbarFlyout>(
      "\ued60",// StrokeErase
      "Clear notes",
      std::vector<std::shared_ptr<IToolbarItem>> {
        // FIXME: pass in ITabView
        std::make_shared<ClearUserInputAction>(
          rootTab, tabView->GetPageIndex()),
        std::make_shared<ClearUserInputAction>(rootTab, AllPages),
        std::make_shared<ClearUserInputAction>(kbs, AllTabs),
      }),
    std::make_shared<ToolbarFlyout>(
      "\ue72c",// Refresh
      "Reload",
      std::vector<std::shared_ptr<IToolbarItem>> {
        std::make_shared<ReloadTabAction>(kbs, rootTab),
        std::make_shared<ReloadTabAction>(kbs, AllTabs),
      }),
  };
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
      },
      .mRight = {
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
    .mSecondary = CreateDropDownItems(kneeboardState, kneeboardView, tabView),
  };
}

}// namespace OpenKneeboard
