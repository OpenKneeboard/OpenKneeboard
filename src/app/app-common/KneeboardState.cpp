// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/APIEventServer.hpp>
#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/DirectInputAdapter.hpp>
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/InterprocessRenderer.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/OpenXRMode.hpp>
#include <OpenKneeboard/PluginStore.hpp>
#include <OpenKneeboard/SHM/ActiveConsumers.hpp>
#include <OpenKneeboard/SteamVRKneeboard.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/TabletInputAdapter.hpp>
#include <OpenKneeboard/TabsList.hpp>
#include <OpenKneeboard/TroubleshootingStore.hpp>
#include <OpenKneeboard/UserAction.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/final_release_deleter.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <algorithm>
#include <string>

namespace OpenKneeboard {

task<std::shared_ptr<KneeboardState>> KneeboardState::Create(
  HWND hwnd,
  audited_ptr<DXResources> dxr) {
  auto ret = shared_with_final_release(new KneeboardState(hwnd, dxr));
  co_await ret->Init();
  co_return ret;
}

task<void> KneeboardState::ReleaseHwndResources() {
  mDirectInput = {};
  co_await this->StopTabletInput();
  co_return;
}

std::shared_ptr<PluginStore> KneeboardState::GetPluginStore() const {
  return mPluginStore;
}

KneeboardState::KneeboardState(HWND hwnd, const audited_ptr<DXResources>& dxr)
  : mHwnd(hwnd), mDXResources(dxr) {
  mQueueFlushedEvent = Win32::or_throw::CreateEventW(
    nullptr,
    /* bManualReset = */ TRUE,
    /* bInitialState = */ FALSE,
    nullptr);
}

task<void> KneeboardState::Init() {
  OPENKNEEBOARD_TraceLoggingCoro("KneeboardState::Init()");

  const scope_success saveMigratedSettings([this]() { this->SaveSettings(); });

  AddEventListener(
    this->evFrameTimerPreEvent,
    std::bind_front(&KneeboardState::BeforeFrame, this));
  AddEventListener(
    this->evFrameTimerPostEvent,
    std::bind_front(&KneeboardState::AfterFrame, this));

  mPluginStore = std::make_shared<PluginStore>();
  mTabsList = co_await TabsList::Create(mDXResources, this, mSettings.mTabs);
  AddEventListener(
    mTabsList->evSettingsChangedEvent,
    std::bind_front(&KneeboardState::SaveSettings, this));
  AddEventListener(mTabsList->evTabsChangedEvent, [this]() {
    const auto tabs = mTabsList->GetTabs();

    for (auto& view: mViews) {
      view->SetTabs(tabs);
    }
    if (mAppWindowView) {
      mAppWindowView->SetTabs(tabs);
    }
  });

  mDirectInput = DirectInputAdapter::Create(mHwnd, mSettings.mDirectInput);
  AddEventListener(mDirectInput->evUserActionEvent, [this](auto action) {
    this->EnqueueOrderedEvent(
      std::bind_front(&KneeboardState::PostUserAction, this, action));
  });
  AddEventListener(
    mDirectInput->evSettingsChangedEvent,
    std::bind_front(&KneeboardState::SaveSettings, this));
  AddEventListener(
    mDirectInput->evAttachedControllersChangedEvent,
    this->evInputDevicesChangedEvent);

  AddEventListener(
    this->evSettingsChangedEvent,
    std::bind_front(&KneeboardState::SetRepaintNeeded, this));

  InitializeViews();
  AcquireExclusiveResources();
}

KneeboardState::~KneeboardState() noexcept {
  OPENKNEEBOARD_TraceLoggingScope("KneeboardState::~KneeboardState()");
  dprint("~KneeboardState()");
}

OpenKneeboard::fire_and_forget KneeboardState::final_release(
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
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "KneeboardState::GetViewRenderInfo()");
  std::vector<ViewRenderInfo> ret;

  const auto count
    = std::min<std::size_t>(mSettings.mViews.mViews.size(), MaxViewCount);
  if (count > mViews.size()) [[unlikely]] {
    fatal("View count mismatch");
  }
  for (size_t i = 0; i < count; ++i) {
    const auto& viewConfig = mSettings.mViews.mViews.at(i);
    if (!viewConfig.mVR.mEnabled) {
      continue;
    }
    const auto view = mViews.at(i);
    const auto contentSize = view->GetPreferredSize();

    const auto layout = view->GetIPCRenderLayout();
    const auto layoutSize
      = (layout.mSize == PixelSize {}) ? ErrorPixelSize : layout.mSize;
    const auto contentLocation = (layout.mSize == PixelSize {})
      ? PixelRect {{0, 0}, ErrorPixelSize}
      : layout.mContent;
    const PixelRect fullLocation {
      {0, 0},
      layoutSize,
    };

    ret.push_back(
      ViewRenderInfo {
        .mView = view,
        .mVR = viewConfig.mVR.Resolve(
          contentSize, fullLocation, contentLocation, mSettings.mViews.mViews),
        .mFullSize = layoutSize,
        .mIsActiveForInput = (i == mInputViewIndex),
      });

    TraceLoggingWriteTagged(
      activity,
      "KneeboardState::GetViewRenderInfo()/View",
      TraceLoggingHexUInt64(
        view->GetRuntimeID().GetTemporaryValue(), "RuntimeID"),
      TraceLoggingGuid(view->GetPersistentGUID(), "PersistentID"),
      OPENKNEEBOARD_TraceLoggingSize2D(
        contentSize.mPixelSize, "PreferredPixelSize"),
      OPENKNEEBOARD_TraceLoggingRect(
        (ret.back().mVR ? ret.back().mVR->mLocationOnTexture : PixelRect {}),
        "VRLocationOnTexture"));
  }

  return ret;
}

std::shared_ptr<KneeboardView> KneeboardState::GetActiveViewForGlobalInput()
  const {
  if (mAppWindowIsForeground) {
    return GetAppWindowView();
  }
  return GetActiveInGameView();
}

std::shared_ptr<KneeboardView> KneeboardState::GetActiveInGameView() const {
  if (mInputViewIndex >= mViews.size()) {
    if (mViews.empty()) {
      dprint.Error("No views in KneeboardState::GetActiveInGameView()");
      return nullptr;
    }
    return mViews.front();
  }
  return mViews.at(mInputViewIndex);
}

void KneeboardState::SetActiveInGameView(KneeboardViewID runtimeID) {
  OPENKNEEBOARD_TraceLoggingScope(
    "KneeboardState::SetActiveInGameView()",
    TraceLoggingValue(runtimeID.GetTemporaryValue(), "RuntimeID"));
  for (int i = 0; i < mViews.size(); ++i) {
    if (mViews.at(i)->GetRuntimeID() != runtimeID) {
      continue;
    }
    if (mInputViewIndex == i) {
      return;
    }
    mViews.at(mInputViewIndex)->PostCursorEvent({});
    TraceLoggingWrite(
      gTraceProvider,
      "KneeboardState::SetActiveInGameView()/SettingView",
      TraceLoggingGuid(mViews.at(i)->GetPersistentGUID(), "GUID"));
    dprint(
      "Giving input focus to view {} at index {}",
      mViews.at(i)->GetPersistentGUID(),
      i);
    mInputViewIndex = i;
    this->SetRepaintNeeded();
    evActiveViewChangedEvent.Emit();
    return;
  }
  dprint(
    "Asked to give input focus to view {:#016x}, but couldn't find it",
    runtimeID.GetTemporaryValue());
}

std::shared_ptr<KneeboardView> KneeboardState::GetAppWindowView() const {
  if (mSettings.mViews.mAppWindowMode == AppWindowViewMode::Independent) {
    return mAppWindowView;
  }
  return GetActiveInGameView();
}

void KneeboardState::NotifyAppWindowIsForeground(bool isForeground) {
  OPENKNEEBOARD_TraceLoggingScope(
    "KneeboardState::NotifyAppWindowIsForeground()",
    TraceLoggingValue(isForeground, "isForeground"));
  mAppWindowIsForeground = isForeground;
}

task<void> KneeboardState::DisposeAsync() noexcept {
  const auto disposing = co_await mDisposal.StartOnce();
  if (!disposing) {
    co_return;
  }

  const auto keepAlive = shared_from_this();

  auto children = mTabsList->GetTabs() | std::views::transform([](auto it) {
                    return std::dynamic_pointer_cast<IHasDisposeAsync>(it);
                  })
    | std::views::filter([](auto it) { return static_cast<bool>(it); })
    | std::views::transform([](auto it) { return it->DisposeAsync(); })
    | std::ranges::to<std::vector>();
  for (auto& child: children) {
    co_await std::move(child);
  }

  // We don't particularly care about things that are still in the queue, but if
  // something has been started, we need to wait for it to finish
  if (mFlushingQueue) {
    co_await winrt::resume_on_signal(mQueueFlushedEvent.get());
  }
}

task<void> KneeboardState::PostUserAction(UserAction action) {
  if (winrt::apartment_context() != mUIThread) {
    dprint("User action in wrong thread!");
    OPENKNEEBOARD_BREAK;
    co_return;
  }
  OPENKNEEBOARD_TraceLoggingCoro(
    "KneeboardState::PostUserAction()",
    TraceLoggingValue(to_string(action).c_str(), "Action"));

  evUserActionEvent.Emit(action);

  switch (action) {
    case UserAction::TOGGLE_VISIBILITY:
    case UserAction::SHOW:
    case UserAction::HIDE:
      mInterprocessRenderer->PostUserAction(action);
      co_return;
    case UserAction::TOGGLE_FORCE_ZOOM: {
      auto& forceZoom = this->mSettings.mVR.mForceZoom;
      forceZoom = !forceZoom;
      this->SaveSettings();
      this->SetRepaintNeeded();
      co_return;
    }
    case UserAction::RECENTER_VR:
      dprint("Recentering");
      this->mSettings.mVR.mRecenterCount++;
      this->SetRepaintNeeded();
      co_return;
    case UserAction::SWAP_FIRST_TWO_VIEWS:
      if (mViews.size() >= 2) {
        mViews.at(0)->SwapState(*mViews.at(1));
      } else {
        dprint(
          "Switching the first two views requires 2 views, but there are {} "
          "views",
          mViews.size());
      }
      co_return;
    case UserAction::PREVIOUS_TAB:
    case UserAction::NEXT_TAB:
    case UserAction::PREVIOUS_PAGE:
    case UserAction::NEXT_PAGE:
    case UserAction::PREVIOUS_BOOKMARK:
    case UserAction::NEXT_BOOKMARK:
    case UserAction::TOGGLE_BOOKMARK:
      co_await GetActiveViewForGlobalInput()->PostUserAction(action);
      co_return;
    case UserAction::PREVIOUS_PROFILE:
      co_await this->SwitchProfile(Direction::Previous);
      co_return;
    case UserAction::NEXT_PROFILE:
      co_await this->SwitchProfile(Direction::Next);
      co_return;
    case UserAction::ENABLE_TINT:
      this->mSettings.mUI.mTint.mEnabled = true;
      this->SaveSettings();
      co_return;
    case UserAction::DISABLE_TINT:
      this->mSettings.mUI.mTint.mEnabled = false;
      this->SaveSettings();
      co_return;
    case UserAction::TOGGLE_TINT:
      this->mSettings.mUI.mTint.mEnabled = !this->mSettings.mUI.mTint.mEnabled;
      this->SaveSettings();
      co_return;
    case UserAction::CYCLE_ACTIVE_VIEW:
      if (this->mViews.size() < 2) {
        co_return;
      }
      this->mInputViewIndex = (this->mInputViewIndex + 1) % this->mViews.size();
      this->evActiveViewChangedEvent.Emit();
      this->SetRepaintNeeded();
      co_return;
    case UserAction::INCREASE_BRIGHTNESS: {
      auto& tint = this->mSettings.mUI.mTint;
      tint.mEnabled = true;
      tint.mBrightness
        = std::clamp(tint.mBrightness + tint.mBrightnessStep, 0.0f, 1.0f);
      this->SaveSettings();
      co_return;
    }
    case UserAction::DECREASE_BRIGHTNESS: {
      auto& tint = this->mSettings.mUI.mTint;
      tint.mEnabled = true;
      tint.mBrightness
        = std::clamp(tint.mBrightness - tint.mBrightnessStep, 0.0f, 1.0f);
      this->SaveSettings();
      co_return;
    }
    case UserAction::REPAINT_NOW:
      this->SetRepaintNeeded();
      co_return;
  }
  // Use `return` instead of `break` above
  OPENKNEEBOARD_BREAK;
}

void KneeboardState::OnGameChangedEvent(
  DWORD processID,
  const std::filesystem::path& game) {
  SHM::ActiveConsumers::Clear();
  if (processID) {
    mCurrentGame = {
      processID,
      game,
      std::chrono::steady_clock::now(),
    };
    mMostRecentGame = mCurrentGame;
  } else {
    mCurrentGame = {};
  }
  this->evGameChangedEvent.Emit(processID, game);
}

void KneeboardState::OnAPIEvent(APIEvent ev) noexcept {
  if (winrt::apartment_context() != mUIThread) {
    dprint("API event in wrong thread!");
    OPENKNEEBOARD_BREAK;
  }
  TroubleshootingStore::Get()->OnAPIEvent(ev);

  mOrderedEventQueue.push(
    std::bind_front(&KneeboardState::ProcessAPIEvent, this, ev));
}

void KneeboardState::EnqueueOrderedEvent(std::function<task<void>()> event) {
  mOrderedEventQueue.push(event);
}

task<void> KneeboardState::FlushOrderedEventQueue(
  std::chrono::time_point<std::chrono::steady_clock> stopAt) {
  if (mFlushingQueue) {
    OPENKNEEBOARD_TraceLoggingWrite(
      "KneeboardState::FlushOrderedEventQueue()/AlreadyFlushing");
    co_await winrt::resume_on_signal(mQueueFlushedEvent.get());
    co_return;
  }
  if (mOrderedEventQueue.empty()) {
    OPENKNEEBOARD_TraceLoggingWrite(
      "KneeboardState::FlushOrderedEventQueue()/Empty");
    co_return;
  }
  OPENKNEEBOARD_TraceLoggingCoro(
    "KneeboardState::FlushOrderedEventQueue()/Flush");

  mFlushingQueue = true;

  ResetEvent(mQueueFlushedEvent.get());
  scope_exit signalCompletion([this]() {
    SetEvent(mQueueFlushedEvent.get());
    mFlushingQueue = false;
  });

  size_t processed = 0;
  while (std::chrono::steady_clock::now() < stopAt
         && !mOrderedEventQueue.empty()) {
    auto it = std::move(mOrderedEventQueue.front());
    mOrderedEventQueue.pop();
    co_await it();
    ++processed;
  }

  OPENKNEEBOARD_TraceLoggingWrite(
    "KneeboardState::FlushOrderedEventQueue()/Stats",
    TraceLoggingValue(processed, "Processed"),
    TraceLoggingValue(mOrderedEventQueue.size(), "Remaining"));
}

task<void> KneeboardState::ProcessAPIEvent(APIEvent ev) noexcept {
  const auto tabs = mTabsList->GetTabs();

  if (ev.name == APIEvent::EVT_REMOTE_USER_ACTION) {
#define IT(ACTION) \
  if (ev.value == #ACTION) { \
    co_await PostUserAction(UserAction::ACTION); \
    co_return; \
  }
    OPENKNEEBOARD_USER_ACTIONS
#undef IT
  }

  if (ev.name == APIEvent::EVT_PLUGIN_TAB_CUSTOM_ACTION) {
    const auto parsed = ev.ParsedValue<PluginTabCustomActionEvent>();
    const auto receiver = this->GetActiveViewForGlobalInput();
    if (receiver) {
      receiver->PostCustomAction(parsed.mActionID, parsed.mExtraData);
    }
    co_return;
  }

  if (ev.name == APIEvent::EVT_SET_TAB_BY_ID) {
    const auto parsed = ev.ParsedValue<SetTabByIDEvent>();
    winrt::guid guid;
    try {
      guid = winrt::guid {parsed.mID};
    } catch (const std::invalid_argument&) {
      dprint("Failed to set tab by ID: '{}' is not a valid GUID", parsed.mID);
      co_return;
    }
    const auto tab = std::ranges::find_if(
      tabs, [guid](const auto& tab) { return tab->GetPersistentID() == guid; });
    if (tab == tabs.end()) {
      dprint(
        "Asked to switch to tab with ID '{}', but can't find it", parsed.mID);
      co_return;
    }
    this->SetCurrentTab(*tab, parsed);
    co_return;
  }

  if (ev.name == APIEvent::EVT_SET_TAB_BY_NAME) {
    const auto parsed = ev.ParsedValue<SetTabByNameEvent>();
    const auto tab = std::ranges::find_if(tabs, [parsed](const auto& tab) {
      return parsed.mName == tab->GetTitle();
    });
    if (tab == tabs.end()) {
      dprint(
        "Asked to switch to tab with name '{}', but can't find it",
        parsed.mName);
      co_return;
    }
    this->SetCurrentTab(*tab, parsed);
    co_return;
  }

  if (ev.name == APIEvent::EVT_SET_TAB_BY_INDEX) {
    const auto parsed = ev.ParsedValue<SetTabByIndexEvent>();
    if (parsed.mIndex >= tabs.size()) {
      dprint(
        "Asked to switch to tab index {}, but there aren't that many tabs",
        parsed.mIndex);
      co_return;
    }
    this->SetCurrentTab(tabs.at(parsed.mIndex), parsed);
    co_return;
  }

  using Profile = ProfileSettings::Profile;
  if (ev.name == APIEvent::EVT_SET_PROFILE_BY_GUID) {
    const auto parsed = ev.ParsedValue<SetProfileByGUIDEvent>();
    if (!mProfiles.mEnabled) {
      dprint("Asked to switch profiles, but profiles are disabled");
    }
    const winrt::guid guid {parsed.mGUID};
    auto it = std::ranges::find(mProfiles.mProfiles, guid, &Profile::mGuid);
    if (it == mProfiles.mProfiles.end()) {
      dprint(
        "Asked to switch to profile with GUID {}, but it doesn't exist", guid);
      co_return;
    }
    ProfileSettings newSettings(mProfiles);
    newSettings.mActiveProfile = guid;
    co_await this->SetProfileSettings(newSettings);
    co_return;
  }

  if (ev.name == APIEvent::EVT_SET_PROFILE_BY_NAME) {
    const auto parsed = ev.ParsedValue<SetProfileByNameEvent>();
    if (!mProfiles.mEnabled) {
      dprint("Asked to switch profiles, but profiles are disabled");
    }
    auto it
      = std::ranges::find(mProfiles.mProfiles, parsed.mName, &Profile::mName);
    if (it == mProfiles.mProfiles.end()) {
      dprint(
        "Asked to switch to profile with ID '{}', but it doesn't exist",
        parsed.mName);
      co_return;
    }
    ProfileSettings newSettings(mProfiles);
    newSettings.mActiveProfile = it->mGuid;
    co_await this->SetProfileSettings(newSettings);
    co_return;
  }

  if (ev.name == APIEvent::EVT_SET_BRIGHTNESS) {
    const auto parsed = ev.ParsedValue<SetBrightnessEvent>();
    auto& tint = this->mSettings.mUI.mTint;
    tint.mEnabled = true;
    switch (parsed.mMode) {
      case SetBrightnessEvent::Mode::Absolute:
        if (parsed.mBrightness < 0 || parsed.mBrightness > 1) {
          dprint(
            "Requested absolute brightness '{}' is outside of range 0 to 1",
            parsed.mBrightness);
          co_return;
        }
        tint.mBrightness = parsed.mBrightness;
        break;
      case SetBrightnessEvent::Mode::Relative:
        if (parsed.mBrightness < -1 || parsed.mBrightness > 1) {
          dprint(
            "Requested relative brightness '{}' is outside of range -1 to 1",
            parsed.mBrightness);
          co_return;
        }
        tint.mBrightness
          = std::clamp(tint.mBrightness + parsed.mBrightness, 0.0f, 1.0f);
        break;
    }
    this->SaveSettings();
    co_return;
  }

  this->evAPIEvent.Emit(ev);
}

void KneeboardState::SetCurrentTab(
  const std::shared_ptr<ITab>& tab,
  const BaseSetTabEvent& extra) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);

  std::shared_ptr<KneeboardView> view;
  if (extra.mKneeboard == 0) {
    view = GetActiveViewForGlobalInput();
  } else if (extra.mKneeboard <= mViews.size()) {
    view = mViews.at(extra.mKneeboard - 1);
  } else {
    dprint(
      "Requested kneeboard index {} does not exist, using active "
      "kneeboard",
      extra.mKneeboard);
    view = GetActiveViewForGlobalInput();
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
      dprint("Requested page index {} >= count {}", pageIndex, pageCount);
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

task<void> KneeboardState::SetViewsSettings(const ViewsSettings& view) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);

  mSettings.mViews = view;
  this->InitializeViews();

  this->SaveSettings();
  this->SetRepaintNeeded();
  co_return;
}

task<void> KneeboardState::SetUISettings(const UISettings& value) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);
  mSettings.mUI = value;
  this->SaveSettings();

  this->SetRepaintNeeded();

  co_return;
}

task<void> KneeboardState::SetVRSettings(const VRSettings& value) {
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
  this->SetRepaintNeeded();

  co_return;
}

task<void> KneeboardState::SetAppSettings(const AppSettings& value) {
  const EventDelay delay;// lock must be released first
  {
    const std::unique_lock lock(*this);
    mSettings.mApp = value;
    this->SaveSettings();
  }
  co_return;
}

TabsList* KneeboardState::GetTabsList() const {
  return mTabsList.get();
}

InterprocessRenderer* KneeboardState::GetInterprocessRenderer() const {
  return mInterprocessRenderer.get();
}

std::shared_ptr<TabletInputAdapter> KneeboardState::GetTabletInputAdapter()
  const {
  return mTabletInput;
}

std::optional<GameProcess> KneeboardState::GetCurrentGame() const {
  return mCurrentGame;
}

std::optional<GameProcess> KneeboardState::GetMostRecentGame() const {
  return mMostRecentGame;
}

ProfileSettings KneeboardState::GetProfileSettings() const {
  return mProfiles;
}

task<void> KneeboardState::SetProfileSettings(
  ProfileSettings profiles,
  std::source_location caller) {
  if (profiles.mActiveProfile != mProfiles.mActiveProfile) {
    dprint(
      "Switching to profile: '{}' - caller: {}",
      profiles.mActiveProfile,
      caller);
  }
  // We want the evSettingsChanged event in particular to be emitted
  // first, so that we don't save
  mSaveSettingsEnabled = false;
  const scope_exit storeFutureChanges(
    [this]() { mSaveSettingsEnabled = true; });

  const EventDelay delay;// lock must be released first
  std::unique_lock lock(*this);

  this->evCurrentProfileChangedEvent.Emit();
  this->evProfileSettingsChangedEvent.Emit();

  const auto oldID = mProfiles.mActiveProfile;
  mProfiles = profiles;
  if (!mProfiles.mEnabled) {
    mProfiles.mActiveProfile = profiles.mDefaultProfile;
  }
  mProfiles.Save();

  const auto newID = mProfiles.mActiveProfile;
  if (oldID == newID) {
    co_return;
  }

  const auto newSettings
    = Settings::Load(mProfiles.mDefaultProfile, mProfiles.mActiveProfile);
  mSettings = newSettings;

  // Avoid partially overwriting the new profile with
  // the old profile

  co_await mTabsList->LoadSettings(newSettings.mTabs);
  co_await this->SetViewsSettings(newSettings.mViews);

  co_await this->SetAppSettings(newSettings.mApp);
  co_await this->SetDoodlesSettings(newSettings.mDoodles);
  mDirectInput->LoadSettings(newSettings.mDirectInput);
  mTabletInput->LoadSettings(newSettings.mTabletInput);
  co_await this->SetVRSettings(newSettings.mVR);

  this->evSettingsChangedEvent.Emit();
  this->SetRepaintNeeded();
}

void KneeboardState::SaveSettings() {
  if (!mSaveSettingsEnabled) {
    return;
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

  mSettings.Save(mProfiles.mDefaultProfile, mProfiles.mActiveProfile);
  evSettingsChangedEvent.Emit();
}

task<void> KneeboardState::SetDoodlesSettings(const DoodleSettings& value) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);
  mSettings.mDoodles = value;
  this->SaveSettings();
  co_return;
}

task<void> KneeboardState::SetTextSettings(const TextSettings& value) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);
  mSettings.mText = value;
  this->SaveSettings();
  co_return;
}

void KneeboardState::StartOpenVRThread() {
  mOpenVRThread = {
    "OKB OpenVR Client",
    [](std::stop_token stopToken) -> task<void> {
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
  AddEventListener(mTabletInput->evUserActionEvent, [this](auto action) {
    this->EnqueueOrderedEvent(
      std::bind_front(&KneeboardState::PostUserAction, this, action));
  });
  AddEventListener(
    mTabletInput->evSettingsChangedEvent,
    std::bind_front(&KneeboardState::SaveSettings, this));
}

task<void> KneeboardState::SetDirectInputSettings(
  const DirectInputSettings& settings) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);

  mDirectInput->LoadSettings(settings);
  co_return;
}

task<void> KneeboardState::SetTabletInputSettings(
  const TabletSettings& settings) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);
  mTabletInput->LoadSettings(settings);
  co_return;
}

task<void> KneeboardState::SetTabsSettings(const nlohmann::json& j) {
  const EventDelay delay;// lock must be released first
  const std::unique_lock lock(*this);
  co_await mTabsList->LoadSettings(j);
}

task<void> KneeboardState::ReleaseExclusiveResources() {
  OPENKNEEBOARD_TraceLoggingCoro("ReleaseExclusiveResources");
  mOpenVRThread = {};
  mInterprocessRenderer = {};
  mAPIEventServer = {};

  co_await this->StopTabletInput();
  co_return;
}

task<void> KneeboardState::StopTabletInput() {
  OPENKNEEBOARD_TraceLoggingCoro("KneeboardState::StopTabletInput()");
  if (mTabletInput) {
    co_await mTabletInput->DisposeAsync();
    mTabletInput = {};
  }
}

void KneeboardState::AcquireExclusiveResources() {
  mInterprocessRenderer = InterprocessRenderer::Create(mDXResources, this);
  if (mSettings.mVR.mEnableSteamVR) {
    StartOpenVRThread();
  }

  StartTabletInput();

  mAPIEventServer = APIEventServer::Create();
  AddEventListener(
    mAPIEventServer->evAPIEvent,
    std::bind_front(&KneeboardState::OnAPIEvent, this));
}

task<void> KneeboardState::SwitchProfile(Direction direction) {
  if (!mProfiles.mEnabled) {
    co_return;
  }
  const auto count = mProfiles.mProfiles.size();
  if (count < 2) {
    co_return;
  }
  const auto profiles = mProfiles.GetSortedProfiles();
  using Profile = ProfileSettings::Profile;
  const auto it
    = std::ranges::find(profiles, mProfiles.mActiveProfile, &Profile::mGuid);
  if (it == profiles.end()) {
    dprint(
      "Current profile '{}' is not in profiles list.",
      mProfiles.mActiveProfile);
    co_return;
  }
  const auto oldIdx = it - profiles.begin();
  const auto nextIdx
    = (direction == Direction::Previous) ? (oldIdx - 1) : (oldIdx + 1);
  if (!mProfiles.mLoopProfiles) {
    if (nextIdx < 0 || nextIdx >= count) {
      dprint("Ignoring profile switch request, looping disabled");
      co_return;
    }
  }

  auto settings = mProfiles;
  settings.mActiveProfile
    = (profiles.begin() + ((nextIdx + count) % count))->mGuid;
  co_await this->SetProfileSettings(settings);
}

bool KneeboardState::IsRepaintNeeded() const {
  return mNeedsRepaint;
}

void KneeboardState::SetRepaintNeeded() {
  OPENKNEEBOARD_TraceLoggingWrite("KneeboardState::SetRepaintNeeded()");
  mNeedsRepaint = true;
}

void KneeboardState::Repainted() {
  mNeedsRepaint = false;
}

void KneeboardState::lock() {
  if (mUniqueLockThread != std::this_thread::get_id()) {
    mMutex.lock();
    mUniqueLockThread = std::this_thread::get_id();
    OPENKNEEBOARD_ASSERT(mUniqueLockDepth == 0);
  }
  ++mUniqueLockDepth;
}

bool KneeboardState::try_lock() {
  if (mUniqueLockThread != std::this_thread::get_id()) {
    const auto success = mMutex.try_lock();
    if (!success) {
      return false;
    }
    mUniqueLockThread = std::this_thread::get_id();
    OPENKNEEBOARD_ASSERT(mUniqueLockDepth == 0);
  }
  ++mUniqueLockDepth;
  return true;
}

void KneeboardState::unlock() {
  OPENKNEEBOARD_ASSERT(mUniqueLockThread == std::this_thread::get_id());
  OPENKNEEBOARD_ASSERT(mUniqueLockDepth > 0);
  if (--mUniqueLockDepth == 0) {
    mUniqueLockThread = std::nullopt;
    mMutex.unlock();
  }
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
      mViews.push_back(
        KneeboardView::Create(mDXResources, this, config.mGuid, config.mName));
    }

    auto& view = mViews.back();

    view->SetTabs(tabs);

    if (config.mDefaultTabID != winrt::guid {}) {
      auto tabIt
        = std::ranges::find(tabs, config.mDefaultTabID, &ITab::GetPersistentID);
      if (tabIt != tabs.end()) {
        dprint(
          "Setting view '{}' ({}) to default tab '{}' ({})",
          config.mName,
          config.mGuid,
          (*tabIt)->GetTitle(),
          config.mDefaultTabID);
        view->SetCurrentTabByRuntimeID((*tabIt)->GetRuntimeID());
      } else {
        dprint(
          "Couldn't find default tab {} for view '{}' ({})",
          config.mDefaultTabID,
          config.mName,
          config.mGuid);
      }
    }

    AddEventListener(
      view->evNeedsRepaintEvent,
      std::bind_front(&KneeboardState::SetRepaintNeeded, this));
  }

  bool viewChanged = false;
  if (mInputViewIndex >= count) {
    mInputViewIndex = (count - 1);
    viewChanged = true;
  }

  switch (mSettings.mViews.mAppWindowMode) {
    case AppWindowViewMode::NoDecision:
    case AppWindowViewMode::ActiveView:
      mAppWindowView = {nullptr};
      break;
    case AppWindowViewMode::Independent:
      if (!mAppWindowView) {
        viewChanged = true;
        mAppWindowView = KneeboardView::Create(
          mDXResources,
          this,
          {},
          "OKB internal independent app window view ('sim racing mode')");
        mAppWindowView->SetTabs(this->GetTabsList()->GetTabs());
        AddEventListener(
          mAppWindowView->evNeedsRepaintEvent,
          std::bind_front(&KneeboardState::SetRepaintNeeded, this));
        viewChanged = true;
      }
  }

  if (viewChanged) {
    evActiveViewChangedEvent.Emit();
  }
  this->SetRepaintNeeded();
}

void KneeboardState::BeforeFrame() {
  OPENKNEEBOARD_TraceLoggingScope("KneeboardState::BeforeFrame()");

  const auto px = SHM::ActiveConsumers::Get().mNonVRPixelSize;
  if (px == mLastNonVRPixelSize) {
    return;
  }
  mLastNonVRPixelSize = px;
  this->SetRepaintNeeded();
}

void KneeboardState::AfterFrame(FramePostEventKind) {
  OPENKNEEBOARD_TraceLoggingScope("KneeboardState::AfterFrame()");

  const auto newActiveViewID = KneeboardViewID::FromTemporaryValue(
    SHM::ActiveConsumers::Get().mActiveInGameViewID);
  if (!newActiveViewID) {
    return;
  }

  const auto activeView = this->GetActiveInGameView();
  if (activeView && activeView->GetRuntimeID() == newActiveViewID) {
    return;
  }

  this->SetActiveInGameView(newActiveViewID);
}

#define IT(cpptype, name) \
  cpptype KneeboardState::Get##name##Settings() const { \
    return mSettings.m##name; \
  } \
  task<void> KneeboardState::Reset##name##Settings() { \
    auto newSettings = mSettings; \
    newSettings.Reset##name##Section( \
      mProfiles.mDefaultProfile, mProfiles.mActiveProfile); \
    co_await this->Set##name##Settings(newSettings.m##name); \
  }
OPENKNEEBOARD_SETTINGS_SECTIONS
#undef IT

}// namespace OpenKneeboard
