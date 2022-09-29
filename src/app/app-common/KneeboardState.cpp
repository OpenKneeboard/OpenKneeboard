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
#include <OpenKneeboard/DirectInputAdapter.h>
#include <OpenKneeboard/GameEventServer.h>
#include <OpenKneeboard/GamesList.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/ITabWithGameEvents.h>
#include <OpenKneeboard/InterprocessRenderer.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/KneeboardView.h>
#include <OpenKneeboard/OpenVRKneeboard.h>
#include <OpenKneeboard/OpenXRMode.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/TabletInputAdapter.h>
#include <OpenKneeboard/TabsList.h>
#include <OpenKneeboard/TroubleshootingStore.h>
#include <OpenKneeboard/UserAction.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>

#include <string>

namespace OpenKneeboard {

KneeboardState::KneeboardState(HWND hwnd, const DXResources& dxr)
  : mDXResources(dxr) {
  if (!mSettings.VR.is_null()) {
    mVRConfig = mSettings.VR;
  }

  mGamesList = std::make_unique<GamesList>(mSettings.Games);
  AddEventListener(
    mGamesList->evSettingsChangedEvent, &KneeboardState::SaveSettings, this);
  AddEventListener(
    mGamesList->evGameChangedEvent, &KneeboardState::OnGameChangedEvent, this);

  mTabsList = std::make_unique<TabsList>(dxr, this, mSettings.Tabs);
  AddEventListener(
    mTabsList->evSettingsChangedEvent, &KneeboardState::SaveSettings, this);
  AddEventListener(mTabsList->evTabsChangedEvent, [this](const auto& tabs) {
    for (auto& view: mViews) {
      view->SetTabs(tabs);
    }
  });

  mViews = {
    std::make_shared<KneeboardView>(dxr, this),
    std::make_shared<KneeboardView>(dxr, this),
  };

  auto tabs = mTabsList->GetTabs();
  for (const auto& viewState: mViews) {
    AddEventListener(viewState->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
    viewState->SetTabs(tabs);
  }

  mInterprocessRenderer
    = std::make_unique<InterprocessRenderer>(mDXResources, this);

  mTabletInput
    = std::make_unique<TabletInputAdapter>(hwnd, this, mSettings.TabletInput);
  AddEventListener(
    mTabletInput->evUserActionEvent, &KneeboardState::OnUserAction, this);
  AddEventListener(
    mTabletInput->evSettingsChangedEvent, &KneeboardState::SaveSettings, this);

  mDirectInput
    = std::make_unique<DirectInputAdapter>(hwnd, mSettings.DirectInput);
  AddEventListener(
    mDirectInput->evUserActionEvent, &KneeboardState::OnUserAction, this);
  AddEventListener(
    mDirectInput->evSettingsChangedEvent, &KneeboardState::SaveSettings, this);
  AddEventListener(
    mDirectInput->evAttachedControllersChangedEvent,
    this->evInputDevicesChangedEvent);

  mGameEventServer = std::make_unique<GameEventServer>();
  AddEventListener(
    mGameEventServer->evGameEvent, &KneeboardState::OnGameEvent, this);
  mGameEventWorker = mGameEventServer->Run();

  if (mVRConfig.mEnableSteamVR) {
    this->StartOpenVRThread();
  }
}

KneeboardState::~KneeboardState() noexcept {
  this->RemoveAllEventListeners();
  mGameEventWorker.Cancel();
  if (mOpenVRThread.get_id() != std::thread::id {}) {
    mOpenVRThread.request_stop();
    mOpenVRThread.join();
  }
}

std::vector<std::shared_ptr<IKneeboardView>>
KneeboardState::GetAllViewsInFixedOrder() const {
  return {mViews.begin(), mViews.end()};
}

std::vector<ViewRenderInfo> KneeboardState::GetViewRenderInfo() const {
  const auto primaryVR = mVRConfig.mPrimaryLayer;
  if (!mSettings.App.mDualKneeboards.mEnabled) {
    return {
      {
        .mView = mViews.at(mFirstViewIndex),
        .mVR = primaryVR,
        .mIsActiveForInput = true,
      },
    };
  }

  auto secondaryVR = primaryVR;
  secondaryVR.mX = -primaryVR.mX;
  // Yaw
  secondaryVR.mRY = -primaryVR.mRY;
  // Roll
  secondaryVR.mRZ = -primaryVR.mRZ;

  return {
    {
      .mView = mViews.at(mFirstViewIndex),
      .mVR = primaryVR,
      .mIsActiveForInput = mFirstViewIndex == mInputViewIndex,
    },
    {
      .mView = mViews.at((mFirstViewIndex + 1) % 2),
      .mVR = secondaryVR,
      .mIsActiveForInput = mFirstViewIndex != mInputViewIndex,
    },
  };
}

std::shared_ptr<IKneeboardView> KneeboardState::GetActiveViewForGlobalInput()
  const {
  return mViews.at(mInputViewIndex);
}

void KneeboardState::OnUserAction(UserAction action) {
  switch (action) {
    case UserAction::TOGGLE_VISIBILITY:
      if (mInterprocessRenderer) {
        mInterprocessRenderer.reset();
      } else {
        mInterprocessRenderer
          = std::make_unique<InterprocessRenderer>(mDXResources, this);
      }
      return;
    case UserAction::TOGGLE_FORCE_ZOOM: {
      auto& flags = this->mVRConfig.mFlags;
      if (static_cast<bool>(flags & VRRenderConfig::Flags::FORCE_ZOOM)) {
        flags &= ~VRRenderConfig::Flags::FORCE_ZOOM;
      } else {
        flags |= VRRenderConfig::Flags::FORCE_ZOOM;
      }
      this->SaveSettings();
      this->evNeedsRepaintEvent.Emit();
      return;
    }
    case UserAction::RECENTER_VR:
      this->mVRConfig.mRecenterCount++;
      this->evNeedsRepaintEvent.Emit();
      return;
    case UserAction::SWITCH_KNEEBOARDS:
      this->SetFirstViewIndex((this->mFirstViewIndex + 1) % 2);
      return;
    case UserAction::PREVIOUS_TAB:
    case UserAction::NEXT_TAB:
    case UserAction::PREVIOUS_PAGE:
    case UserAction::NEXT_PAGE:
      mViews.at(mInputViewIndex)->PostUserAction(action);
      return;
  }
  OPENKNEEBOARD_BREAK;
}
void KneeboardState::SetFirstViewIndex(uint8_t index) {
  const auto inputIsFirst = this->mFirstViewIndex == this->mInputViewIndex;
  this->mFirstViewIndex
    = std::min<uint8_t>(index, mSettings.App.mDualKneeboards.mEnabled ? 1 : 0);
  if (inputIsFirst) {
    this->mInputViewIndex = this->mFirstViewIndex;
  } else {
    this->mInputViewIndex = std::min<uint8_t>(
      index, mSettings.App.mDualKneeboards.mEnabled ? 1 : 0);
  }
  this->evNeedsRepaintEvent.Emit();
  this->evViewOrderChangedEvent.Emit();
}

void KneeboardState::OnGameChangedEvent(
  DWORD processID,
  std::shared_ptr<GameInstance> game) {
  if (processID && game) {
    mCurrentGame = {processID, {game}};
  } else {
    mCurrentGame = {};
  }
  this->evGameChangedEvent.Emit(processID, game);
}

void KneeboardState::OnGameEvent(const GameEvent& ev) {
  TroubleshootingStore::Get()->OnGameEvent(ev);

  if (ev.name == GameEvent::EVT_SET_INPUT_FOCUS) {
    const auto viewID = std::stoull(ev.value);
    for (int i = 0; i < mViews.size(); ++i) {
      if (mViews.at(i)->GetRuntimeID() != viewID) {
        continue;
      }
      if (mInputViewIndex == i) {
        return;
      }
      dprintf("Giving input focus to view {:#016x} at index {}", viewID, i);
      mInputViewIndex = i;
      evNeedsRepaintEvent.Emit();
      evViewOrderChangedEvent.Emit();
      return;
    }
    dprintf(
      "Asked to give input focus to view {:#016x}, but couldn't find it",
      viewID);
    return;
  }

  if (ev.name == GameEvent::EVT_REMOTE_USER_ACTION) {
#define IT(ACTION) \
  if (ev.value == #ACTION) { \
    OnUserAction(UserAction::ACTION); \
    return; \
  }
    OPENKNEEBOARD_USER_ACTIONS
#undef IT
  }

  for (auto tab: mTabsList->GetTabs()) {
    auto receiver = std::dynamic_pointer_cast<ITabWithGameEvents>(tab);
    if (receiver) {
      receiver->PostGameEvent(ev);
    }
  }
}

std::vector<std::shared_ptr<UserInputDevice>> KneeboardState::GetInputDevices()
  const {
  std::vector<std::shared_ptr<UserInputDevice>> devices;
  for (const auto& subDevices:
       {mTabletInput->GetDevices(), mDirectInput->GetDevices()}) {
    devices.reserve(devices.size() + subDevices.size());
    for (const auto& device: subDevices) {
      devices.push_back(device);
    }
  }
  return devices;
}

FlatConfig KneeboardState::GetFlatConfig() const {
  return mSettings.NonVR;
}

void KneeboardState::SetFlatConfig(const FlatConfig& value) {
  mSettings.NonVR = value;
  this->SaveSettings();
  this->evNeedsRepaintEvent.Emit();
}

VRConfig KneeboardState::GetVRConfig() const {
  return mVRConfig;
}

void KneeboardState::SetVRConfig(const VRConfig& value) {
  if (value.mEnableSteamVR != mVRConfig.mEnableSteamVR) {
    if (!value.mEnableSteamVR) {
      mOpenVRThread.request_stop();
    } else {
      this->StartOpenVRThread();
    }
  }

  if (value.mOpenXRMode != mVRConfig.mOpenXRMode) {
    [=]() -> winrt::fire_and_forget {
      co_await SetOpenXRModeWithHelperProcess(
        value.mOpenXRMode, {mVRConfig.mOpenXRMode});
    }();
  }

  if (
    (value.mFlags & VRConfig::Flags::GAZE_INPUT_FOCUS)
    != (mVRConfig.mFlags & VRConfig::Flags::GAZE_INPUT_FOCUS)) {
    mInputViewIndex = mFirstViewIndex;
    this->evViewOrderChangedEvent.Emit();
  }

  mVRConfig = value;
  this->SaveSettings();
  this->evNeedsRepaintEvent.Emit();
}

AppSettings KneeboardState::GetAppSettings() const {
  return mSettings.App;
}

void KneeboardState::SetAppSettings(const AppSettings& value) {
  mSettings.App = value;
  this->SaveSettings();
  if (!value.mDualKneeboards.mEnabled) {
    this->SetFirstViewIndex(0);
  }
}

GamesList* KneeboardState::GetGamesList() const {
  return mGamesList.get();
}

TabsList* KneeboardState::GetTabsList() const {
  return mTabsList.get();
}

std::optional<RunningGame> KneeboardState::GetCurrentGame() const {
  return mCurrentGame;
}

void KneeboardState::SaveSettings() {
  if (mGamesList) {
    mSettings.Games = mGamesList->GetSettings();
  }
  if (mTabsList) {
    mSettings.Tabs = mTabsList->GetSettings();
  }
  if (mTabletInput) {
    mSettings.TabletInput = mTabletInput->GetSettings();
  }
  if (mDirectInput) {
    mSettings.DirectInput = mDirectInput->GetSettings();
  }

  mSettings.VR = mVRConfig;

  mSettings.Save();
  evSettingsChangedEvent.Emit();
}

DoodleSettings KneeboardState::GetDoodleSettings() {
  return mSettings.Doodle;
}

void KneeboardState::SetDoodleSettings(const DoodleSettings& value) {
  mSettings.Doodle = value;
  this->SaveSettings();
}

void KneeboardState::StartOpenVRThread() {
  mOpenVRThread = std::jthread([](std::stop_token stopToken) {
    SetThreadDescription(GetCurrentThread(), L"OpenVR Thread");
    OpenVRKneeboard().Run(stopToken);
  });
}

}// namespace OpenKneeboard
