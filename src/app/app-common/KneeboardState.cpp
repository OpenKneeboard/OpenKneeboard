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
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/DirectInputAdapter.h>
#include <OpenKneeboard/GameEventServer.h>
#include <OpenKneeboard/GameInstance.h>
#include <OpenKneeboard/GamesList.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/InterprocessRenderer.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/KneeboardView.h>
#include <OpenKneeboard/OpenXRMode.h>
#include <OpenKneeboard/SHM/ActiveConsumers.h>
#include <OpenKneeboard/SteamVRKneeboard.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/TabletInputAdapter.h>
#include <OpenKneeboard/TabsList.h>
#include <OpenKneeboard/TroubleshootingStore.h>
#include <OpenKneeboard/UserAction.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/final_release_deleter.h>
#include <OpenKneeboard/scope_guard.h>

#include <algorithm>
#include <string>

namespace OpenKneeboard {

std::shared_ptr<KneeboardState> KneeboardState::Create(
  HWND hwnd,
  const audited_ptr<DXResources>& dxr) {
  return shared_with_final_release(new KneeboardState(hwnd, dxr));
}

winrt::Windows::Foundation::IAsyncAction
KneeboardState::ReleaseHwndResources() {
  mTabletInput = {};
  mDirectInput = {};
  co_return;
}

KneeboardState::KneeboardState(HWND hwnd, const audited_ptr<DXResources>& dxr)
  : mHwnd(hwnd), mDXResources(dxr) {
  const scope_guard saveMigratedSettings([this]() { this->SaveSettings(); });

  AddEventListener(
    this->evNeedsRepaintEvent, [this]() { this->mNeedsRepaint = true; });
  AddEventListener(this->evFrameTimerPreEvent, [this]() {
    const auto px = SHM::ActiveConsumers::Get().mNonVRPixelSize;
    if (px == mLastNonVRPixelSize) {
      return;
    }
    mLastNonVRPixelSize = px;
    this->evNeedsRepaintEvent.Emit();
  });

  mGamesList = std::make_unique<GamesList>(this, mSettings.mGames);
  AddEventListener(
    mGamesList->evSettingsChangedEvent,
    std::bind_front(&KneeboardState::SaveSettings, this));
  AddEventListener(
    mGamesList->evGameChangedEvent,
    std::bind_front(&KneeboardState::OnGameChangedEvent, this));

  mTabsList = std::make_unique<TabsList>(dxr, this, mSettings.mTabs);
  AddEventListener(
    mTabsList->evSettingsChangedEvent,
    std::bind_front(&KneeboardState::SaveSettings, this));
  AddEventListener(mTabsList->evTabsChangedEvent, [this](const auto& tabs) {
    for (auto& view: mViews) {
      view->SetTabs(tabs);
    }
  });

  mDirectInput = DirectInputAdapter::Create(hwnd, mSettings.mDirectInput);
  AddEventListener(
    mDirectInput->evUserActionEvent,
    std::bind_front(&KneeboardState::PostUserAction, this));
  AddEventListener(
    mDirectInput->evSettingsChangedEvent,
    std::bind_front(&KneeboardState::SaveSettings, this));
  AddEventListener(
    mDirectInput->evAttachedControllersChangedEvent,
    this->evInputDevicesChangedEvent);

  AddEventListener(this->evSettingsChangedEvent, this->evNeedsRepaintEvent);

  InitializeViews();
  AcquireExclusiveResources();
}

KneeboardState::~KneeboardState() noexcept {
  OPENKNEEBOARD_TraceLoggingScope("KneeboardState::~KneeboardState()");
  dprint("~KneeboardState()");
}

winrt::fire_and_forget KneeboardState::final_release(
  std::unique_ptr<KneeboardState> self) {
  TraceLoggingWrite(gTraceProvider, "KneeboardState::final_release()");
  self->RemoveAllEventListeners();
  co_await self->ReleaseExclusiveResources();

  // Implied, but let's get some perf tracing on the member's destructors
  self = {};
  TraceLoggingWrite(gTraceProvider, "KneeboardState::~final_release()");
}

std::vector<std::shared_ptr<KneeboardView>>
KneeboardState::GetAllViewsInFixedOrder() const {
  return {mViews.begin(), mViews.end()};
}

std::vector<ViewRenderInfo> KneeboardState::GetViewRenderInfo() const {
  std::vector<ViewRenderInfo> ret;

  const size_t count = mSettings.mViews.mViews.size();
  if (count != mViews.size()) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL("View count mismatch");
  }
  for (size_t i = 0; i < count; ++i) {
    const auto& viewConfig = mSettings.mViews.mViews.at(i);
    const auto view = mViews.at(i);
    const auto contentSize = view->GetPreferredSize();

    const auto layout = view->GetIPCRenderLayout();
    const auto layoutSize
      = (layout.mSize == PixelSize {}) ? ErrorRenderSize : layout.mSize;
    const auto contentLocation = (layout.mSize == PixelSize {})
      ? PixelRect {{0, 0}, ErrorRenderSize}
      : layout.mContent;
    const PixelRect fullLocation {
      {0, 0},
      layoutSize,
    };

    ret.push_back(ViewRenderInfo {
      .mView = view,
      .mVR = viewConfig.mVR.Resolve(
        contentSize, fullLocation, contentLocation, mSettings.mViews.mViews),
      .mNonVR = viewConfig.mNonVR.Resolve(
        contentSize, fullLocation, contentLocation, mSettings.mViews.mViews),
      .mFullSize = layoutSize,
      .mIsActiveForInput = (i == mInputViewIndex),
    });
  }

  return ret;
}

std::shared_ptr<KneeboardView> KneeboardState::GetActiveViewForGlobalInput()
  const {
  return mViews.at(mInputViewIndex);
}

void KneeboardState::PostUserAction(UserAction action) {
  if (winrt::apartment_context() != mUIThread) {
    dprint("User action in wrong thread!");
    OPENKNEEBOARD_BREAK;
    return;
  }
  OPENKNEEBOARD_TraceLoggingScope(
    "KneeboardState::PostUserAction()",
    TraceLoggingValue(to_string(action).c_str(), "Action"));

  evUserActionEvent.Emit(action);

  switch (action) {
    case UserAction::TOGGLE_VISIBILITY:
    case UserAction::SHOW:
    case UserAction::HIDE:
      mInterprocessRenderer->PostUserAction(action);
      return;
    case UserAction::TOGGLE_FORCE_ZOOM: {
      auto& forceZoom = this->mSettings.mVR.mForceZoom;
      forceZoom = !forceZoom;
      this->SaveSettings();
      this->evNeedsRepaintEvent.Emit();
      return;
    }
    case UserAction::RECENTER_VR:
      dprint("Recentering");
      this->mSettings.mVR.mRecenterCount++;
      this->evNeedsRepaintEvent.Emit();
      return;
    case UserAction::SWAP_FIRST_TWO_VIEWS:
      if (mViews.size() >= 2) {
        mViews.at(0)->SwapState(*mViews.at(1));
      } else {
        dprintf(
          "Switching the first two views requires 2 views, but there are {} "
          "views",
          mViews.size());
      }
      return;
    case UserAction::PREVIOUS_TAB:
    case UserAction::NEXT_TAB:
    case UserAction::PREVIOUS_PAGE:
    case UserAction::NEXT_PAGE:
    case UserAction::PREVIOUS_BOOKMARK:
    case UserAction::NEXT_BOOKMARK:
    case UserAction::TOGGLE_BOOKMARK:
    case UserAction::RELOAD_CURRENT_TAB:
      mViews.at(mInputViewIndex)->PostUserAction(action);
      return;
    case UserAction::PREVIOUS_PROFILE:
      this->SwitchProfile(Direction::Previous);
      return;
    case UserAction::NEXT_PROFILE:
      this->SwitchProfile(Direction::Next);
      return;
    case UserAction::ENABLE_TINT:
      this->mSettings.mApp.mTint.mEnabled = true;
      this->SaveSettings();
      return;
    case UserAction::DISABLE_TINT:
      this->mSettings.mApp.mTint.mEnabled = false;
      this->SaveSettings();
      return;
    case UserAction::TOGGLE_TINT:
      this->mSettings.mApp.mTint.mEnabled
        = !this->mSettings.mApp.mTint.mEnabled;
      this->SaveSettings();
      return;
    case UserAction::CYCLE_ACTIVE_VIEW:
      if (this->mViews.size() < 2) {
        return;
      }
      this->mInputViewIndex = (this->mInputViewIndex + 1) % this->mViews.size();
      this->evActiveViewChangedEvent.Emit();
      this->evNeedsRepaintEvent.Emit();
      return;
    case UserAction::INCREASE_BRIGHTNESS: {
      auto& tint = this->mSettings.mApp.mTint;
      tint.mEnabled = true;
      tint.mBrightness
        = std::clamp(tint.mBrightness + tint.mBrightnessStep, 0.0f, 1.0f);
      this->SaveSettings();
      return;
    }
    case UserAction::DECREASE_BRIGHTNESS: {
      auto& tint = this->mSettings.mApp.mTint;
      tint.mEnabled = true;
      tint.mBrightness
        = std::clamp(tint.mBrightness - tint.mBrightnessStep, 0.0f, 1.0f);
      this->SaveSettings();
      return;
    }
    case UserAction::REPAINT_NOW:
      this->evNeedsRepaintEvent.Emit();
      return;
  }
  // Use `return` instead of `break` above
  OPENKNEEBOARD_BREAK;
}

void KneeboardState::OnGameChangedEvent(
  DWORD processID,
  const std::shared_ptr<GameInstance>& game) {
  SHM::ActiveConsumers::Clear();
  if (processID && game) {
    mCurrentGame = {processID, {game}};
  } else {
    mCurrentGame = {};
  }
  this->evGameChangedEvent.Emit(processID, game);
}

void KneeboardState::OnGameEvent(const GameEvent& ev) noexcept {
  if (winrt::apartment_context() != mUIThread) {
    dprint("Game event in wrong thread!");
    OPENKNEEBOARD_BREAK;
  }
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
      mViews.at(mInputViewIndex)->PostCursorEvent({});
      dprintf("Giving input focus to view {:#016x} at index {}", viewID, i);
      mInputViewIndex = i;
      evNeedsRepaintEvent.Emit();
      evActiveViewChangedEvent.Emit();
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

  if (ev.name == GameEvent::EVT_SET_BRIGHTNESS) {
    const auto parsed = ev.ParsedValue<SetBrightnessEvent>();
    auto& tint = this->mSettings.mApp.mTint;
    tint.mEnabled = true;
    switch (parsed.mMode) {
      case SetBrightnessEvent::Mode::Absolute:
        if (parsed.mBrightness < 0 || parsed.mBrightness > 1) {
          dprintf(
            "Requested absolute brightness '{}' is outside of range 0 to 1",
            parsed.mBrightness);
          return;
        }
        tint.mBrightness = parsed.mBrightness;
        break;
      case SetBrightnessEvent::Mode::Relative:
        if (parsed.mBrightness < -1 || parsed.mBrightness > 1) {
          dprintf(
            "Requested relative brightness '{}' is outside of range -1 to 1",
            parsed.mBrightness);
          return;
        }
        tint.mBrightness
          = std::clamp(tint.mBrightness + parsed.mBrightness, 0.0f, 1.0f);
        break;
    }
    this->SaveSettings();
    return;
  }

  this->evGameEvent.Emit(ev);
}

void KneeboardState::SetCurrentTab(
  const std::shared_ptr<ITab>& tab,
  const BaseSetTabEvent& extra) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);

  std::shared_ptr<KneeboardView> view;
  if (extra.mKneeboard == 0) {
    view = mViews.at(mInputViewIndex);
  } else if (extra.mKneeboard <= mViews.size()) {
    view = mViews.at(extra.mKneeboard - 1);
  } else {
    dprintf(
      "Requested kneeboard index {} does not exist, using active "
      "kneeboard",
      extra.mKneeboard);
    view = mViews.at(mInputViewIndex);
  }
  view->SetCurrentTabByRuntimeID(tab->GetRuntimeID());
  const auto pageIDs = tab->GetPageIDs();
  const auto pageCount = pageIDs.size();
  if (extra.mPageNumber != 0 && pageCount > 1) {
    const auto pageIndex = extra.mPageNumber - 1;
    if (pageIndex < pageCount) {
      this->GetActiveViewForGlobalInput()->GetCurrentTabView()->SetPageID(
        pageIDs.at(pageIndex));
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

void KneeboardState::SetViewsSettings(const ViewsConfig& view) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);

  mSettings.mViews = view;
  this->InitializeViews();

  this->SaveSettings();
  this->evNeedsRepaintEvent.Emit();
}

void KneeboardState::SetVRSettings(const VRConfig& value) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);

  if (value.mEnableSteamVR != mSettings.mVR.mEnableSteamVR) {
    if (!value.mEnableSteamVR) {
      mOpenVRThread = {};
    } else {
      this->StartOpenVRThread();
    }
  }

  if (value.mEnableGazeInputFocus != mSettings.mVR.mEnableGazeInputFocus) {
    mViews.at(mInputViewIndex)->PostCursorEvent({});
    mInputViewIndex = 0;
    this->evActiveViewChangedEvent.Emit();
  }

  mSettings.mVR = value;
  this->SaveSettings();
  this->evNeedsRepaintEvent.Emit();
}

void KneeboardState::SetAppSettings(const AppSettings& value) {
  const EventDelay delay;// lock must be released first
  {
    const std::unique_lock lock(*this);
    mSettings.mApp = value;
    this->SaveSettings();
  }
}

GamesList* KneeboardState::GetGamesList() const {
  return mGamesList.get();
}

TabsList* KneeboardState::GetTabsList() const {
  return mTabsList.get();
}

std::shared_ptr<TabletInputAdapter> KneeboardState::GetTabletInputAdapter()
  const {
  return mTabletInput;
}

std::optional<RunningGame> KneeboardState::GetCurrentGame() const {
  return mCurrentGame;
}

ProfileSettings KneeboardState::GetProfileSettings() const {
  return mProfiles;
}

void KneeboardState::SetProfileSettings(const ProfileSettings& profiles) {
  // We want the evSettingsChanged event in particular to be emitted
  // first, so that we don't save
  mSaveSettingsEnabled = false;
  const scope_guard storeFutureChanges(
    [this]() { mSaveSettingsEnabled = true; });

  const EventDelay delay;// lock must be released first
  std::unique_lock lock(*this);

  this->evCurrentProfileChangedEvent.Emit();
  this->evProfileSettingsChangedEvent.Emit();

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
  mSettings = newSettings;
  lock.unlock();

  // Avoid partially overwriting the new profile with
  // the old profile

  mTabsList->LoadSettings(newSettings.mTabs);
  this->SetViewsSettings(newSettings.mViews);

  this->SetAppSettings(newSettings.mApp);
  this->SetDoodlesSettings(newSettings.mDoodles);
  mGamesList->LoadSettings(newSettings.mGames);
  mTabletInput->LoadSettings(newSettings.mTabletInput);
  this->SetVRSettings(newSettings.mVR);

  this->evSettingsChangedEvent.Emit();
  this->evNeedsRepaintEvent.Emit();
}

void KneeboardState::SaveSettings() {
  if (!mSaveSettingsEnabled) {
    return;
  }

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
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);
  mSettings.mDoodles = value;
  this->SaveSettings();
}

void KneeboardState::SetTextSettings(const TextSettings& value) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);
  mSettings.mText = value;
  this->SaveSettings();
}

void KneeboardState::StartOpenVRThread() {
  mOpenVRThread = {
    "OpenVR Thread",
    [](std::stop_token stopToken) -> winrt::Windows::Foundation::IAsyncAction {
      TraceLoggingWrite(gTraceProvider, "OpenVRThread/Start");
      {
        SteamVRKneeboard steamVR;
        co_await steamVR.Run(stopToken);
      }
      TraceLoggingWrite(gTraceProvider, "OpenVRThread/Stop");
    },
  };
}

void KneeboardState::StartTabletInput() {
  mTabletInput
    = TabletInputAdapter::Create(mHwnd, this, mSettings.mTabletInput);
  AddEventListener(
    mTabletInput->evDeviceConnectedEvent, this->evInputDevicesChangedEvent);
  AddEventListener(
    mTabletInput->evUserActionEvent,
    std::bind_front(&KneeboardState::PostUserAction, this));
  AddEventListener(
    mTabletInput->evSettingsChangedEvent,
    std::bind_front(&KneeboardState::SaveSettings, this));
}

void KneeboardState::SetDirectInputSettings(
  const DirectInputSettings& settings) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);

  mDirectInput->LoadSettings(settings);
}

void KneeboardState::SetTabletInputSettings(const TabletSettings& settings) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);
  mTabletInput->LoadSettings(settings);
}

void KneeboardState::SetGamesSettings(const nlohmann::json& j) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);
  mGamesList->LoadSettings(j);
}

void KneeboardState::SetTabsSettings(const nlohmann::json& j) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);
  mTabsList->LoadSettings(j);
}

winrt::Windows::Foundation::IAsyncAction
KneeboardState::ReleaseExclusiveResources() {
  OPENKNEEBOARD_TraceLoggingScope("ReleaseExclusiveResources");
  mOpenVRThread = {};
  mInterprocessRenderer = {};
  mTabletInput = {};
  mGameEventServer = {};
  co_return;
}

void KneeboardState::AcquireExclusiveResources() {
  mInterprocessRenderer = InterprocessRenderer::Create(mDXResources, this);
  if (mSettings.mVR.mEnableSteamVR) {
    StartOpenVRThread();
  }

  StartTabletInput();

  mGameEventServer = GameEventServer::Create();
  AddEventListener(
    mGameEventServer->evGameEvent,
    std::bind_front(&KneeboardState::OnGameEvent, this));
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

bool KneeboardState::IsRepaintNeeded() const {
  return mNeedsRepaint;
}

void KneeboardState::Repainted() {
  mNeedsRepaint = false;
}

void KneeboardState::lock() {
  mMutex.lock();
}

bool KneeboardState::try_lock() {
  return mMutex.try_lock();
}

void KneeboardState::unlock() {
  mMutex.unlock();
}

void KneeboardState::lock_shared() {
  mMutex.lock_shared();
}

bool KneeboardState::try_lock_shared() {
  return mMutex.try_lock_shared();
}

void KneeboardState::unlock_shared() {
  mMutex.unlock_shared();
}

void KneeboardState::InitializeViews() {
  const auto oldViews = mViews;
  const auto count = mSettings.mViews.mViews.size();
  auto tabs = mTabsList->GetTabs();

  mViews.clear();
  for (size_t i = 0; i < count; ++i) {
    const auto& config = mSettings.mViews.mViews.at(i);
    const auto it = std::ranges::find(
      oldViews, config.mGuid, &KneeboardView::GetPersistentGUID);

    if (it != oldViews.end()) {
      mViews.push_back(*it);
    } else {
      mViews.push_back(KneeboardView::Create(mDXResources, this, config.mGuid));
    }

    auto& view = mViews.back();

    view->SetTabs(tabs);

    if (config.mDefaultTabID != winrt::guid {}) {
      auto tabIt
        = std::ranges::find(tabs, config.mDefaultTabID, &ITab::GetPersistentID);
      if (tabIt != tabs.end()) {
        dprintf(
          "Setting view '{}' ({}) to default tab '{}' ({})",
          config.mName,
          config.mGuid,
          (*tabIt)->GetTitle(),
          config.mDefaultTabID);
        view->SetCurrentTabByRuntimeID((*tabIt)->GetRuntimeID());
      } else {
        dprintf(
          "Couldn't find default tab {} for view '{}' ({})",
          config.mDefaultTabID,
          config.mName,
          config.mGuid);
      }
    }

    AddEventListener(view->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
  }

  if (mInputViewIndex >= count) {
    mInputViewIndex = (count - 1);
    evActiveViewChangedEvent.Emit();
  }

  evNeedsRepaintEvent.Emit();
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
