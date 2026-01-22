// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/SwitchProfileAction.hpp>

namespace OpenKneeboard {

SwitchProfileAction::SwitchProfileAction(
  KneeboardState* kbs,
  const winrt::guid& profileID,
  const std::string& profileName)
  : ToolbarAction({}, profileName),
    mKneeboardState(kbs),
    mProfileID(profileID) {}

SwitchProfileAction::~SwitchProfileAction() { this->RemoveAllEventListeners(); }

bool SwitchProfileAction::IsChecked() const {
  return mKneeboardState->GetProfileSettings().mActiveProfile == mProfileID;
}

bool SwitchProfileAction::IsEnabled() const { return true; }

task<void> SwitchProfileAction::Execute() {
  auto profileSettings = mKneeboardState->GetProfileSettings();
  profileSettings.mActiveProfile = mProfileID;
  co_await mKneeboardState->SetProfileSettings(profileSettings);
  co_return;
}

}// namespace OpenKneeboard
