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
#include <OpenKneeboard/IPageSourceWithDeveloperTools.hpp>
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/TabDeveloperToolsAction.hpp>
#include <OpenKneeboard/TabView.hpp>

namespace OpenKneeboard {

TabDeveloperToolsAction::TabDeveloperToolsAction(
  KneeboardState* kneeboard,
  KneeboardViewID kneeboardView,
  const std::shared_ptr<TabView>& tabView)
  : ToolbarAction("\uEC7A", _("Developer tools")),
    mKneeboard(kneeboard),
    mKneeboardView(kneeboardView),
    mTabView(tabView) {
  AddEventListener(tabView->evPageChangedEvent, this->evStateChangedEvent);
}

TabDeveloperToolsAction::~TabDeveloperToolsAction() {
  this->RemoveAllEventListeners();
}

bool TabDeveloperToolsAction::IsEnabled() const {
  auto tv = mTabView.lock();
  if (!tv) {
    return false;
  }

  auto tab = tv->GetRootTab().lock();
  if (!tab) {
    return false;
  }
  auto inner = std::dynamic_pointer_cast<IPageSourceWithDeveloperTools>(tab);
  if (!inner) {
    return false;
  }
  return inner->HasDeveloperTools(tv->GetPageID());
}

task<void> TabDeveloperToolsAction::Execute() {
  auto tv = mTabView.lock();
  if (!tv) {
    co_return;
  }

  auto tab = tv->GetRootTab().lock();
  if (!tab) {
    co_return;
  }
  auto page = std::dynamic_pointer_cast<IPageSourceWithDeveloperTools>(tab);
  if (!page) {
    co_return;
  }
  page->OpenDeveloperToolsWindow(mKneeboardView, tv->GetPageID());
}

}// namespace OpenKneeboard
