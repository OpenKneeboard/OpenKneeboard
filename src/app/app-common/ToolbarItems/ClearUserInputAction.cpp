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
#include <OpenKneeboard/IPageSourceWithCursorEvents.h>
#include <OpenKneeboard/ITabView.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/TabsList.h>

namespace OpenKneeboard {

ClearUserInputAction::ClearUserInputAction(
  const std::shared_ptr<ITabView>& tab,
  CurrentPage_t)
  : ToolbarAction({}, _("This page")), mMode(Mode::CurrentPage), mTabView(tab) {
  SubscribeToTabView();
}

ClearUserInputAction::ClearUserInputAction(
  const std::shared_ptr<ITabView>& tab,
  AllPages_t)
  : ToolbarAction({}, _("All pages")), mMode(Mode::ThisTab), mTabView(tab) {
  SubscribeToTabView();
}

void ClearUserInputAction::SubscribeToTabView() {
  if (!mTabView) {
    throw std::logic_error("Bad tab view");
  }

  AddEventListener(mTabView->evPageChangedEvent, this->evStateChangedEvent);
  AddEventListener(
    mTabView->evAvailableFeaturesChangedEvent, this->evStateChangedEvent);
}

ClearUserInputAction::ClearUserInputAction(KneeboardState* kbs, AllTabs_t)
  : ToolbarAction({}, _("All tabs")),
    mMode(Mode::AllTabs),
    mKneeboardState(kbs) {
}

ClearUserInputAction::~ClearUserInputAction() {
  this->RemoveAllEventListeners();
}

bool ClearUserInputAction::IsEnabled() const {
  auto wce = mTabView
    ? std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(mTabView->GetTab())
    : nullptr;
  switch (mMode) {
    case Mode::CurrentPage:
      return wce && wce->CanClearUserInput(mTabView->GetPageIndex());
    case Mode::ThisTab:
      return wce && wce->CanClearUserInput();
    case Mode::AllTabs:
      return true;
  }
  OPENKNEEBOARD_UNREACHABLE;
}

void ClearUserInputAction::Execute() {
  if (mMode == Mode::AllTabs) {
    for (const auto& tab: mKneeboardState->GetTabsList()->GetTabs()) {
      auto wce = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(tab);
      if (wce) {
        wce->ClearUserInput();
      }
    }
    return;
  }

  auto wce = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(
    mTabView->GetTab());
  if (!wce) {
    return;
  }

  switch (mMode) {
    case Mode::ThisTab:
      wce->ClearUserInput();
      return;
    case Mode::CurrentPage:
      wce->ClearUserInput(mTabView->GetPageIndex());
      return;
    case Mode::AllTabs:
      OPENKNEEBOARD_UNREACHABLE;
  }
  OPENKNEEBOARD_UNREACHABLE;
}

std::string_view ClearUserInputAction::GetConfirmationTitle() const {
  switch (mMode) {
    case Mode::CurrentPage:
      return _("Clear this page?");
    case Mode::ThisTab:
      return _("Clear this tab?");
    case Mode::AllTabs:
      return _("Clear all tabs?");
  }
  OPENKNEEBOARD_UNREACHABLE;
}

std::string_view ClearUserInputAction::GetConfirmationDescription() const {
  switch (mMode) {
    case Mode::CurrentPage:
      return _("Do you want to clear all notes and drawings on this page?");
    case Mode::ThisTab:
      return _(
        "Do you want to clear all notes and drawings on ALL PAGES in the "
        "current tab?");
    case Mode::AllTabs:
      return _(
        "Do you want to clear all notes and drawings on ALL PAGES in ALL "
        "TABS?");
  }
  OPENKNEEBOARD_UNREACHABLE;
}

std::string_view ClearUserInputAction::GetConfirmButtonLabel() const {
  return _("Clear");
}
std::string_view ClearUserInputAction::GetCancelButtonLabel() const {
  return _("Cancel");
}

}// namespace OpenKneeboard
