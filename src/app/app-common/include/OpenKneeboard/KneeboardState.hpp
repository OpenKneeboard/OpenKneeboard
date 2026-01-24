// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/IHasDisposeAsync.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/ProfileSettings.hpp>
#include <OpenKneeboard/RunnerThread.hpp>
#include <OpenKneeboard/SHM.hpp>
#include <OpenKneeboard/Settings.hpp>
#include <OpenKneeboard/VRSettings.hpp>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/single_threaded_lockable.hpp>
#include <OpenKneeboard/task.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Foundation.h>

#include <memory>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace OpenKneeboard {

enum class UserAction;
class DirectInputAdapter;
class PluginStore;
class KneeboardView;
class InterprocessRenderer;
class KneeboardView;
class ITab;
class TabletInputAdapter;
class TabsList;
class UserInputDevice;
struct BaseSetTabEvent;
class APIEventServer;

struct ViewRenderInfo {
  std::shared_ptr<KneeboardView> mView;
  std::optional<SHM::VRLayer> mVR;
  PixelSize mFullSize;
  bool mIsActiveForInput = false;
};

struct GameProcess {
  DWORD mProcessID = 0;
  std::filesystem::path mLastSeenPath;
  std::chrono::steady_clock::time_point mSince {};
};

enum class FramePostEventKind {
  /// IsRepaintNeeded() was false
  WithRepaint,
  /// IsRepaintNeeded() was true
  WithoutRepaint,
};

class KneeboardState final
  : private EventReceiver,
    public IHasDisposeAsync,
    public std::enable_shared_from_this<KneeboardState> {
 public:
  KneeboardState() = delete;
  static task<audited_ptr<KneeboardState>> Create(
    HWND mainWindow,
    audited_ptr<DXResources>);
  ~KneeboardState() noexcept;
  [[nodiscard]] virtual task<void> DisposeAsync() noexcept override;

  std::shared_ptr<KneeboardView> GetActiveViewForGlobalInput() const;
  std::shared_ptr<KneeboardView> GetActiveInGameView() const;
  void SetActiveInGameView(KneeboardViewID runtimeID);

  std::shared_ptr<KneeboardView> GetAppWindowView() const;
  std::vector<std::shared_ptr<KneeboardView>> GetAllViewsInFixedOrder() const;
  std::vector<ViewRenderInfo> GetViewRenderInfo() const;

  Event<> evFrameTimerPreEvent;
  Event<FramePostEventKind> evFrameTimerPostEvent;
  Event<> evSettingsChangedEvent;
  Event<> evProfileSettingsChangedEvent;
  Event<> evCurrentProfileChangedEvent;
  Event<> evActiveViewChangedEvent;
  Event<> evInputDevicesChangedEvent;
  Event<UserAction> evUserActionEvent;
  Event<APIEvent> evAPIEvent;
  Event<DWORD, std::filesystem::path> evGameChangedEvent;

  std::vector<std::shared_ptr<UserInputDevice>> GetInputDevices() const;

  std::optional<GameProcess> GetCurrentGame() const;
  std::optional<GameProcess> GetMostRecentGame() const;

  std::shared_ptr<PluginStore> GetPluginStore() const;

  std::shared_ptr<TabletInputAdapter> GetTabletInputAdapter() const;

  ProfileSettings GetProfileSettings() const;
  [[nodiscard]] task<void> SetProfileSettings(
    ProfileSettings,
    std::source_location caller = std::source_location::current());

  void NotifyAppWindowIsForeground(bool isForeground);

  TabsList* GetTabsList() const;
  InterprocessRenderer* GetInterprocessRenderer() const;

  task<void> ReleaseExclusiveResources();
  task<void> StopTabletInput();
  task<void> ReleaseHwndResources();
  void AcquireExclusiveResources();
#define IT(cpptype, name) \
  cpptype Get##name##Settings() const; \
  task<void> Set##name##Settings(const cpptype&); \
  task<void> Reset##name##Settings();
  OPENKNEEBOARD_SETTINGS_SECTIONS
#undef IT

  void SaveSettings();

  [[nodiscard]] task<void> PostUserAction(UserAction action);

  bool IsRepaintNeeded() const;
  void SetRepaintNeeded();
  void Repainted();

  /** Implement `Lockable`; use `std::unique_lock`.
   *
   * This:
   * - is a recursive lock
   * - currently has no ordering issues with DXResources::lock()
   * - should not be used *instead of* DXResources::lock()
   *
   * Use DXResources::lock() for DirectX ownership, but use this
   * for application-level locks.
   **/
  void lock();
  bool try_lock();
  void unlock();
  void lock_shared();
  bool try_lock_shared();
  void unlock_shared();

  auto GetDXResources() const noexcept { return mDXResources; }

  task<void> FlushOrderedEventQueue(
    std::chrono::time_point<std::chrono::steady_clock> stopAt);
  void EnqueueOrderedEvent(std::function<task<void>()>);

 private:
  KneeboardState(HWND mainWindow, const audited_ptr<DXResources>&);
  [[nodiscard]] task<void> Init();

  DisposalState mDisposal;

  std::shared_mutex mMutex;
  // In `lock()`/`try_lock()`/`unlock()`, we allow multiple unique_locks in the
  // same thread; this is so that `SetProfileSettings()` can call other
  // `SetFooSettings()`.
  std::optional<std::thread::id> mUniqueLockThread {};
  std::size_t mUniqueLockDepth = 0;

  bool mNeedsRepaint;
  winrt::apartment_context mUIThread;
  HWND mHwnd;
  audited_ptr<DXResources> mDXResources;
  ProfileSettings mProfiles {ProfileSettings::Load()};
  Settings mSettings {
    Settings::Load(mProfiles.mDefaultProfile, mProfiles.mActiveProfile)};

  uint8_t mInputViewIndex = 0;
  std::vector<std::shared_ptr<KneeboardView>> mViews;

  std::shared_ptr<KneeboardView> mAppWindowView;
  bool mAppWindowIsForeground {false};

  PixelSize mLastNonVRPixelSize {};

  std::shared_ptr<TabsList> mTabsList;
  std::shared_ptr<InterprocessRenderer> mInterprocessRenderer;
  // Initalization and destruction order must match as they both use
  // SetWindowLongPtr
  std::shared_ptr<DirectInputAdapter> mDirectInput;
  std::shared_ptr<TabletInputAdapter> mTabletInput;

  std::shared_ptr<PluginStore> mPluginStore;

  std::shared_ptr<APIEventServer> mAPIEventServer;
  RunnerThread mOpenVRThread;
  std::optional<GameProcess> mCurrentGame;
  std::optional<GameProcess> mMostRecentGame;

  std::queue<std::function<task<void>()>> mOrderedEventQueue;
  bool mFlushingQueue = false;
  winrt::handle mQueueFlushedEvent;

  bool mSaveSettingsEnabled = true;

  void OnGameChangedEvent(DWORD processID, const std::filesystem::path&);
  void OnAPIEvent(APIEvent) noexcept;
  task<void> ProcessAPIEvent(APIEvent) noexcept;

  void BeforeFrame();
  void AfterFrame(FramePostEventKind);

  void StartOpenVRThread();
  void StartTabletInput();

  void SetCurrentTab(
    const std::shared_ptr<ITab>& tab,
    const BaseSetTabEvent& metadata);

  enum class Direction {
    Previous,
    Next,
  };
  [[nodiscard]] task<void> SwitchProfile(Direction);

  void InitializeViews();
};

}// namespace OpenKneeboard
