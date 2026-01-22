// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/ClearUserInputAction.hpp>
#include <OpenKneeboard/IPageSourceWithCursorEvents.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/TabsList.hpp>

namespace OpenKneeboard {

ClearUserInputAction::ClearUserInputAction(
  KneeboardState* kbs,
  const std::shared_ptr<TabView>& tab,
  CurrentPage_t)
  : ToolbarAction({}, _("This page")),
    mMode(Mode::CurrentPage),
    mKneeboardState(kbs),
    mTabView(tab) {
  SubscribeToEvents();
}

ClearUserInputAction::ClearUserInputAction(
  KneeboardState* kbs,
  const std::shared_ptr<TabView>& tab,
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
  auto wce = tabView ? std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(
                         tabView->GetTab().lock())
                     : nullptr;
  switch (mMode) {
    case Mode::CurrentPage:
      return wce && tabView && wce->CanClearUserInput(tabView->GetPageID());
    case Mode::ThisTab:
      return wce && wce->CanClearUserInput();
    case Mode::AllTabs:
      for (const auto& tab: mKneeboardState->GetTabsList()->GetTabs()) {
        auto tabWce =
          std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(tab);
        if (tabWce && tabWce->CanClearUserInput()) {
          return true;
        }
      }
      return false;
  }
  std::unreachable();
}

task<void> ClearUserInputAction::Execute() {
  if (mMode == Mode::AllTabs) {
    for (const auto& tab: mKneeboardState->GetTabsList()->GetTabs()) {
      auto wce = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(tab);
      if (wce) {
        wce->ClearUserInput();
      }
    }
    co_return;
  }

  auto tabView = mTabView.lock();
  if (!tabView) {
    co_return;
  }

  auto wce = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(
    tabView->GetTab().lock());
  if (!wce) {
    co_return;
  }

  switch (mMode) {
    case Mode::ThisTab:
      wce->ClearUserInput();
      co_return;
    case Mode::CurrentPage:
      wce->ClearUserInput(tabView->GetPageID());
      co_return;
    case Mode::AllTabs:
      std::unreachable();
  }
  std::unreachable();
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
  std::unreachable();
}

std::string_view ClearUserInputAction::GetConfirmationDescription() const {
  return _("This will erase all notes, drawings, or other annotations.");
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
  std::unreachable();
}
std::string_view ClearUserInputAction::GetCancelButtonLabel() const {
  return _("Cancel");
}

}// namespace OpenKneeboard
