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
#include <OpenKneeboard/scope_guard.h>

#include <algorithm>
#include <string>

namespace OpenKneeboard {

KneeboardState::KneeboardState(HWND hwnd, const DXResources& dxr)
  : mHwnd(hwnd), mDXResources(dxr) {
  const scope_guard saveMigratedSettings([this]() { this->SaveSettings(); });
  mGamesList = std::make_unique<GamesList>(mSettings.mGames);
  AddEventListener(
    mGamesList->evSettingsChangedEvent, &KneeboardState::SaveSettings, this);
  AddEventListener(
    mGamesList->evGameChangedEvent, &KneeboardState::OnGameChangedEvent, this);

  mTabsList = std::make_unique<TabsList>(dxr, this, mSettings.mTabs);
  AddEventListener(
    mTabsList->evSettingsChangedEvent, &KneeboardState::SaveSettings, this);
  AddEventListener(mTabsList->evTabsChangedEvent, [this](const auto& tabs) {
    for (auto& view: mViews) {
      view->SetTabs(tabs);
    }
  });

  mViews = {
    KneeboardView::Create(dxr, this),
    KneeboardView::Create(dxr, this),
  };

  auto tabs = mTabsList->GetTabs();
  for (const auto& viewState: mViews) {
    AddEventListener(viewState->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
    viewState->SetTabs(tabs);
  }

  mDirectInput = DirectInputAdapter::Create(hwnd, mSettings.mDirectInput);
  AddEventListener(
    mDirectInput->evUserActionEvent, &KneeboardState::PostUserAction, this);
  AddEventListener(
    mDirectInput->evSettingsChangedEvent, &KneeboardState::SaveSettings, this);
  AddEventListener(
    mDirectInput->evAttachedControllersChangedEvent,
    this->evInputDevicesChangedEvent);

  mGameEventServer = std::make_unique<GameEventServer>();
  AddEventListener(
    mGameEventServer->evGameEvent, &KneeboardState::OnGameEvent, this);

  AcquireExclusiveResources();
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
  const auto primaryVR = mSettings.mVR.mPrimaryLayer;
  if (!mSettings.mApp.mDualKneeboards.mEnabled) {
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
      .mView = mViews.at((mFirstViewIndex + 1) % mViews.size()),
      .mVR = secondaryVR,
      .mIsActiveForInput = mFirstViewIndex != mInputViewIndex,
    },
  };
}

std::shared_ptr<IKneeboardView> KneeboardState::GetActiveViewForGlobalInput()
  const {
  return mViews.at(mInputViewIndex);
}

void KneeboardState::PostUserAction(UserAction action) {
  switch (action) {
    case UserAction::TOGGLE_VISIBILITY:
      if (mInterprocessRenderer) {
        mInterprocessRenderer.reset();
      } else {
        mInterprocessRenderer
          = std::make_unique<InterprocessRenderer>(mDXResources, this);
      }
      return;
    case UserAction::HIDE:
      mInterprocessRenderer.reset();
      return;
    case UserAction::SHOW:
      if (mInterprocessRenderer) {
        return;
      }
      mInterprocessRenderer
        = std::make_unique<InterprocessRenderer>(mDXResources, this);
      return;
    case UserAction::TOGGLE_FORCE_ZOOM: {
      auto& forceZoom = this->mSettings.mVR.mForceZoom;
      forceZoom = !forceZoom;
      this->SaveSettings();
      this->evNeedsRepaintEvent.Emit();
      return;
    }
    case UserAction::RECENTER_VR:
      this->mSettings.mVR.mRecenterCount++;
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
    case UserAction::PREVIOUS_PROFILE:
      this->SwitchProfile(Direction::Previous);
      return;
    case UserAction::NEXT_PROFILE:
      this->SwitchProfile(Direction::Next);
      return;
  }
  // Use `return` instead of `break` above
  OPENKNEEBOARD_BREAK;
}
void KneeboardState::SetFirstViewIndex(uint8_t index) {
  const auto inputIsFirst = this->mFirstViewIndex == this->mInputViewIndex;
  this->mFirstViewIndex
    = std::min<uint8_t>(index, mSettings.mApp.mDualKneeboards.mEnabled ? 1 : 0);
  if (inputIsFirst) {
    this->mInputViewIndex = this->mFirstViewIndex;
  } else {
    this->mInputViewIndex = std::min<uint8_t>(
      index, mSettings.mApp.mDualKneeboards.mEnabled ? 1 : 0);
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

void KneeboardState::OnGameEvent(const GameEvent& ev) noexcept {
  TroubleshootingStore::Get()->OnGameEvent(ev);

  const auto tabs = mTabsList->GetTabs();

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
    PostUserAction(UserAction::ACTION); \
    return; \
  }
    OPENKNEEBOARD_USER_ACTIONS
#undef IT
  }

  if (ev.name == GameEvent::EVT_SET_TAB_BY_ID) {
    const auto parsed = ev.ParsedValue<SetTabByIDEvent>();
    winrt::guid guid;
    try {
      guid = winrt::guid {parsed.mID};
    } catch (const std::invalid_argument&) {
      dprintf("Failed to set tab by ID: '{}' is not a valid GUID", parsed.mID);
      return;
    }
    const auto tab = std::ranges::find_if(
      tabs, [guid](const auto& tab) { return tab->GetPersistentID() == guid; });
    if (tab == tabs.end()) {
      dprintf(
        "Asked to switch to tab with ID '{}', but can't find it", parsed.mID);
      return;
    }
    this->SetCurrentTab(*tab, parsed);
    return;
  }

  if (ev.name == GameEvent::EVT_SET_TAB_BY_NAME) {
    const auto parsed = ev.ParsedValue<SetTabByNameEvent>();
    const auto tab = std::ranges::find_if(tabs, [parsed](const auto& tab) {
      return parsed.mName == tab->GetTitle();
    });
    if (tab == tabs.end()) {
      dprintf(
        "Asked to switch to tab with name '{}', but can't find it",
        parsed.mName);
      return;
    }
    this->SetCurrentTab(*tab, parsed);
    return;
  }

  if (ev.name == GameEvent::EVT_SET_TAB_BY_INDEX) {
    const auto parsed = ev.ParsedValue<SetTabByIndexEvent>();
    if (parsed.mIndex >= tabs.size()) {
      dprintf(
        "Asked to switch to tab index {}, but there aren't that many tabs",
        parsed.mIndex);
      return;
    }
    this->SetCurrentTab(tabs.at(parsed.mIndex), parsed);
    return;
  }

  if (ev.name == GameEvent::EVT_SET_PROFILE_BY_ID) {
    const auto parsed = ev.ParsedValue<SetProfileByIDEvent>();
    if (!mProfiles.mEnabled) {
      dprint("Asked to switch profiles, but profiles are disabled");
    }
    if (!mProfiles.mProfiles.contains(parsed.mID)) {
      dprintf(
        "Asked to switch to profile with ID '{}', but it doesn't exist",
        parsed.mID);
      return;
    }
    ProfileSettings newSettings(mProfiles);
    newSettings.mActiveProfile = parsed.mID;
    this->SetProfileSettings(newSettings);
    return;
  }

  if (ev.name == GameEvent::EVT_SET_PROFILE_BY_NAME) {
    const auto parsed = ev.ParsedValue<SetProfileByNameEvent>();
    if (!mProfiles.mEnabled) {
      dprint("Asked to switch profiles, but profiles are disabled");
    }
    auto it = std::ranges::find_if(
      mProfiles.mProfiles,
      [name = parsed.mName](const auto& p) { return p.second.mName == name; });
    if (it == mProfiles.mProfiles.end()) {
      dprintf(
        "Asked to switch to profile with ID '{}', but it doesn't exist",
        parsed.mName);
      return;
    }
    ProfileSettings newSettings(mProfiles);
    newSettings.mActiveProfile = it->first;
    this->SetProfileSettings(newSettings);
    return;
  }

  for (auto tab: tabs) {
    auto receiver = std::dynamic_pointer_cast<ITabWithGameEvents>(tab);
    if (receiver) {
      receiver->PostGameEvent(ev);
    }
  }
  this->evGameEvent.Emit(ev);
}

void KneeboardState::SetCurrentTab(
  const std::shared_ptr<ITab>& tab,
  const BaseSetTabEvent& extra) {
  auto view = this->GetActiveViewForGlobalInput();
  switch (extra.mKneeboard) {
    case 0:
      // Primary, as above.
      break;
    case 1:
      view = mViews.at(mFirstViewIndex);
      break;
    case 2:
      view = mViews.at((mFirstViewIndex + 1) % mViews.size());
      break;
    default:
      dprintf(
        "Requested kneeboard index {} does not exist, using active "
        "kneeboard",
        extra.mKneeboard);
      break;
  }
  view->SetCurrentTabByRuntimeID(tab->GetRuntimeID());
  const auto pageCount = tab->GetPageCount();
  if (extra.mPageNumber != 0 && pageCount > 1) {
    const auto pageIndex = extra.mPageNumber - 1;
    if (pageIndex < pageCount) {
      this->GetActiveViewForGlobalInput()->GetCurrentTabView()->SetPageIndex(
        pageIndex);
    } else {
      dprintf("Requested page index {} >= count {}", pageIndex, pageCount);
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

void KneeboardState::SetNonVRSettings(const FlatConfig& value) {
  mSettings.mNonVR = value;
  this->SaveSettings();
  this->evNeedsRepaintEvent.Emit();
}

void KneeboardState::SetVRSettings(const VRConfig& value) {
  if (value.mEnableSteamVR != mSettings.mVR.mEnableSteamVR) {
    if (!value.mEnableSteamVR) {
      mOpenVRThread.request_stop();
    } else {
      this->StartOpenVRThread();
    }
  }

  if (value.mEnableGazeInputFocus != mSettings.mVR.mEnableGazeInputFocus) {
    mInputViewIndex = mFirstViewIndex;
    this->evViewOrderChangedEvent.Emit();
  }

  mSettings.mVR = value;
  this->SaveSettings();
  this->evNeedsRepaintEvent.Emit();
}

void KneeboardState::SetAppSettings(const AppSettings& value) {
  mSettings.mApp = value;
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

ProfileSettings KneeboardState::GetProfileSettings() const {
  return mProfiles;
}

void KneeboardState::SetProfileSettings(const ProfileSettings& profiles) {
  const scope_guard emitProfileSettingsChanged(
    [&]() { this->evProfileSettingsChangedEvent.Emit(); });

  const auto oldID = mProfiles.mActiveProfile;
  mProfiles = profiles;
  if (!mProfiles.mEnabled) {
    mProfiles.mActiveProfile = "default";
  }
  mProfiles.Save();

  const auto newID = mProfiles.mActiveProfile;
  if (oldID == newID) {
    return;
  }

  const auto newSettings = Settings::Load(newID);
  this->SetAppSettings(newSettings.mApp);
  // DirectInput
  this->SetDoodlesSettings(newSettings.mDoodles);
  mGamesList->LoadSettings(newSettings.mGames);
  this->SetNonVRSettings(newSettings.mNonVR);
  mTabletInput->LoadSettings(newSettings.mTabletInput);
  mTabsList->LoadSettings(newSettings.mTabs);
  this->SetVRSettings(newSettings.mVR);
  mSettings = newSettings;

  this->evSettingsChangedEvent.Emit();
  this->evCurrentProfileChangedEvent.Emit();
  this->evNeedsRepaintEvent.Emit();
}

void KneeboardState::SaveSettings() {
  if (mGamesList) {
    mSettings.mGames = mGamesList->GetSettings();
  }
  if (mTabsList) {
    mSettings.mTabs = mTabsList->GetSettings();
  }
  if (mTabletInput) {
    mSettings.mTabletInput = mTabletInput->GetSettings();
  }
  if (mDirectInput) {
    mSettings.mDirectInput = mDirectInput->GetSettings();
  }

  mSettings.Save(mProfiles.mActiveProfile);
  evSettingsChangedEvent.Emit();
}

void KneeboardState::SetDoodlesSettings(const DoodleSettings& value) {
  mSettings.mDoodles = value;
  this->SaveSettings();
}

void KneeboardState::StartOpenVRThread() {
  mOpenVRThread = std::jthread([](std::stop_token stopToken) {
    SetThreadDescription(GetCurrentThread(), L"OpenVR Thread");
    OpenVRKneeboard().Run(stopToken);
  });
}

void KneeboardState::StartTabletInput() {
  mTabletInput
    = std::make_unique<TabletInputAdapter>(mHwnd, this, mSettings.mTabletInput);
  AddEventListener(
    mTabletInput->evUserActionEvent, &KneeboardState::PostUserAction, this);
  AddEventListener(
    mTabletInput->evSettingsChangedEvent, &KneeboardState::SaveSettings, this);
}

void KneeboardState::SetDirectInputSettings(
  const DirectInputSettings& settings) {
  mDirectInput->LoadSettings(settings);
}

void KneeboardState::SetTabletInputSettings(const TabletSettings& settings) {
  mTabletInput->LoadSettings(settings);
}

void KneeboardState::SetGamesSettings(const nlohmann::json& j) {
  mGamesList->LoadSettings(j);
}

void KneeboardState::SetTabsSettings(const nlohmann::json& j) {
  mTabsList->LoadSettings(j);
}

void KneeboardState::ReleaseExclusiveResources() {
  mOpenVRThread = {};
  mInterprocessRenderer = {};
  mTabletInput = {};
  mGameEventWorker.Cancel();
}

void KneeboardState::AcquireExclusiveResources() {
  mGameEventWorker = mGameEventServer->Run();
  mInterprocessRenderer
    = std::make_unique<InterprocessRenderer>(mDXResources, this);
  if (mSettings.mVR.mEnableSteamVR) {
    StartOpenVRThread();
  }

  StartTabletInput();
}

void KneeboardState::SwitchProfile(Direction direction) {
  if (!mProfiles.mEnabled) {
    return;
  }
  const auto count = mProfiles.mProfiles.size();
  if (count < 2) {
    return;
  }
  const auto profiles = mProfiles.GetSortedProfiles();
  const auto it = std::ranges::find_if(
    profiles,
    [id = mProfiles.mActiveProfile](const auto& it) { return it.mID == id; });
  if (it == profiles.end()) {
    dprintf(
      "Current profile '{}' is not in profiles list.",
      mProfiles.mActiveProfile);
    return;
  }
  const auto oldIdx = it - profiles.begin();
  const auto nextIdx
    = (direction == Direction::Previous) ? (oldIdx - 1) : (oldIdx + 1);
  if (!mProfiles.mLoopProfiles) {
    if (nextIdx < 0 || nextIdx >= count) {
      dprint("Ignoring profile switch request, looping disabled");
      return;
    }
  }

  auto settings = mProfiles;
  settings.mActiveProfile = profiles.at((nextIdx + count) % count).mID;
  this->SetProfileSettings(settings);
}

#define IT(cpptype, name) \
  cpptype KneeboardState::Get##name##Settings() const { \
    return mSettings.m##name; \
  } \
  void KneeboardState::Reset##name##Settings() { \
    auto newSettings = mSettings; \
    newSettings.Reset##name##Section(mProfiles.mActiveProfile); \
    this->Set##name##Settings(newSettings.m##name); \
  }
OPENKNEEBOARD_SETTINGS_SECTIONS
#undef IT

}// namespace OpenKneeboard
