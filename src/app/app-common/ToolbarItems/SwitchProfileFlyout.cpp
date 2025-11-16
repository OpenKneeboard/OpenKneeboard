// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/SwitchProfileAction.hpp>
#include <OpenKneeboard/SwitchProfileFlyout.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/TabsList.hpp>

namespace OpenKneeboard {

SwitchProfileFlyout::SwitchProfileFlyout(KneeboardState* kbs)
  : mKneeboardState(kbs) {
  this->AddEventListener(
    mKneeboardState->evProfileSettingsChangedEvent, this->evStateChangedEvent);
}

SwitchProfileFlyout::~SwitchProfileFlyout() {
  this->RemoveAllEventListeners();
}

std::string_view SwitchProfileFlyout::GetGlyph() const {
  return {};
}
std::string_view SwitchProfileFlyout::GetLabel() const {
  return _("Switch profile");
}

bool SwitchProfileFlyout::IsEnabled() const {
  return true;
}

bool SwitchProfileFlyout::IsVisible() const {
  return mKneeboardState->GetProfileSettings().mEnabled;
}

std::vector<std::shared_ptr<IToolbarItem>> SwitchProfileFlyout::GetSubItems()
  const {
  std::vector<std::shared_ptr<IToolbarItem>> ret;
  for (const auto& profile:
       mKneeboardState->GetProfileSettings().GetSortedProfiles()) {
    ret.push_back(std::make_shared<SwitchProfileAction>(
      mKneeboardState, profile.mGuid, profile.mName));
  }
  return ret;
}

}// namespace OpenKneeboard
