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
  KneeboardState* kbs,
  const std::shared_ptr<ITabView>& tab,
  CurrentPage_t)
  : ToolbarAction({}, _("This page")),
    mMode(Mode::CurrentPage),
    mKneeboardState(kbs),
    mTabView(tab) {
  SubscribeToEvents();
}

ClearUserInputAction::ClearUserInputAction(
  KneeboardState* kbs,
  const std::shared_ptr<ITabView>& tab,
  AllPages_t)
  : ToolbarAction({}, _("All pages")),
    mMode(Mode::ThisTab),
    mKneeboardState(kbs),
    mTabView(tab) {
  SubscribeToEvents();
}

void ClearUserInputAction::SubscribeToEvents() {
  for (const auto& tab: mKneeboardState->GetTabsList()->GetTabs()) {
    AddEventListener(
      tab->evAvailableFeaturesChangedEvent, this->evStateChangedEvent);
  }
  AddEventListener(
    mKneeboardState->GetTabsList()->evTabsChangedEvent,
    this->evStateChangedEvent);

  auto tabView = mTabView.lock();

  if (!tabView) {
    return;
  }

  AddEventListener(tabView->evPageChangedEvent, this->evStateChangedEvent);
  AddEventListener(
    tabView->evAvailableFeaturesChangedEvent, this->evStateChangedEvent);
}

ClearUserInputAction::ClearUserInputAction(KneeboardState* kbs, AllTabs_t)
  : ToolbarAction({}, _("All tabs")),
    mMode(Mode::AllTabs),
    mKneeboardState(kbs) {
  SubscribeToEvents();
}

ClearUserInputAction::~ClearUserInputAction() {
  this->RemoveAllEventListeners();
}

bool ClearUserInputAction::IsEnabled() const {
  auto tabView = mTabView.lock();
  auto wce = tabView
    ? std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(tabView->GetTab())
    : nullptr;
  switch (mMode) {
    case Mode::CurrentPage:
      return wce && tabView && wce->CanClearUserInput(tabView->GetPageID());
    case Mode::ThisTab:
      return wce && wce->CanClearUserInput();
    case Mode::AllTabs:
      for (const auto& tab: mKneeboardState->GetTabsList()->GetTabs()) {
        auto tabWce
          = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(tab);
        if (tabWce && tabWce->CanClearUserInput()) {
          return true;
        }
      }
      return false;
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

  auto tabView = mTabView.lock();
  if (!tabView) {
    return;
  }

  auto wce
    = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(tabView->GetTab());
  if (!wce) {
    return;
  }

  switch (mMode) {
    case Mode::ThisTab:
      wce->ClearUserInput();
      return;
    case Mode::CurrentPage:
      wce->ClearUserInput(tabView->GetPageID());
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
      return _("Clear all pages in this tab?");
    case Mode::AllTabs:
      return _("Clear all pages in every tab?");
  }
  OPENKNEEBOARD_UNREACHABLE;
}

std::string_view ClearUserInputAction::GetConfirmationDescription() const {
  return _("This will erase all notes, drawings, or other annotations.");
  OPENKNEEBOARD_UNREACHABLE;
}

std::string_view ClearUserInputAction::GetConfirmButtonLabel() const {
  switch (mMode) {
    case Mode::CurrentPage:
      return _("Clear page");
    case Mode::ThisTab:
      return _("Clear tab");
    case Mode::AllTabs:
      return _("Clear all pages and tabs");
  }
  OPENKNEEBOARD_UNREACHABLE;
}
std::string_view ClearUserInputAction::GetCancelButtonLabel() const {
  return _("Cancel");
}

}// namespace OpenKneeboard
