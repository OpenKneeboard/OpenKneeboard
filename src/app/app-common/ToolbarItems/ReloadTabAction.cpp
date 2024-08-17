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
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/ReloadTabAction.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/TabsList.hpp>

namespace OpenKneeboard {

ReloadTabAction::ReloadTabAction(
  KneeboardState* kbs,
  const std::shared_ptr<TabView>& tab)
  : ToolbarAction({}, _("This tab")),
    mMode(Mode::ThisTab),
    mKneeboardState(kbs),
    mTabView(tab) {
  if (!tab) {
    throw std::logic_error("Bad itab");
  }
}

ReloadTabAction::ReloadTabAction(KneeboardState* kbs, AllTabs_t)
  : ToolbarAction({}, _("All tabs")),
    mMode(Mode::AllTabs),
    mKneeboardState(kbs) {
}

ReloadTabAction::~ReloadTabAction() {
  this->RemoveAllEventListeners();
}

bool ReloadTabAction::IsEnabled() const {
  return true;
}

task<void> ReloadTabAction::Execute() {
  if (mMode == Mode::ThisTab) {
    auto tv = mTabView.lock();
    if (!tv) {
      co_return;
    }
    auto tab = tv->GetTab();
    if (tab) {
      co_await tab->Reload();
    }
    co_return;
  }

  for (auto& tab: mKneeboardState->GetTabsList()->GetTabs()) {
    co_await tab->Reload();
  }
}

std::string_view ReloadTabAction::GetConfirmationTitle() const {
  if (mMode == Mode::ThisTab) {
    return _("Reload this tab?");
  }
  return _("Reload OpenKneeboard?");
}

std::string_view ReloadTabAction::GetConfirmationDescription() const {
  return _(
    "This will erase all notes and drawings, and any information that was "
    "added since OpenKneeboard started.");
}

std::string_view ReloadTabAction::GetConfirmButtonLabel() const {
  if (mMode == Mode::ThisTab) {
    return _("Reload this tab");
  }
  return _("Reload every tab");
}

std::string_view ReloadTabAction::GetCancelButtonLabel() const {
  return _("Cancel");
}

}// namespace OpenKneeboard
