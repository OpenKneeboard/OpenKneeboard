// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
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
    auto tab = tv->GetTab().lock();
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
