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
#pragma once

#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/ProfileSettings.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/Settings.h>
#include <OpenKneeboard/VRConfig.h>
#include <shims/winrt/base.h>
#include <winrt/Windows.Foundation.h>

#include <memory>
#include <thread>
#include <vector>

namespace OpenKneeboard {

enum class UserAction;
class DirectInputAdapter;
class GamesList;
class IKneeboardView;
class InterprocessRenderer;
class KneeboardView;
class ITab;
class TabletInputAdapter;
class TabsList;
class UserInputDevice;
struct GameEvent;
struct GameInstance;
class GameEventServer;

struct ViewRenderInfo {
  std::shared_ptr<IKneeboardView> mView;
  VRLayerConfig mVR;
  bool mIsActiveForInput = false;
};

struct RunningGame {
  DWORD mProcessID = 0;
  std::weak_ptr<GameInstance> mGameInstance;
};

class KneeboardState final : private EventReceiver {
 public:
  KneeboardState() = delete;
  KneeboardState(HWND mainWindow, const DXResources&);
  ~KneeboardState() noexcept;

  std::shared_ptr<IKneeboardView> GetActiveViewForGlobalInput() const;
  std::vector<std::shared_ptr<IKneeboardView>> GetAllViewsInFixedOrder() const;
  std::vector<ViewRenderInfo> GetViewRenderInfo() const;

  Event<> evFrameTimerEvent;
  Event<> evNeedsRepaintEvent;
  Event<> evSettingsChangedEvent;
  Event<> evProfileSettingsChangedEvent;
  Event<> evCurrentProfileChangedEvent;
  Event<> evViewOrderChangedEvent;
  Event<> evInputDevicesChangedEvent;
  Event<DWORD, std::shared_ptr<GameInstance>> evGameChangedEvent;

  std::vector<std::shared_ptr<UserInputDevice>> GetInputDevices() const;

  GamesList* GetGamesList() const;
  std::optional<RunningGame> GetCurrentGame() const;

  ProfileSettings GetProfileSettings() const;
  void SetProfileSettings(const ProfileSettings&);

  TabsList* GetTabsList() const;

#define IT(cpptype, name) \
  cpptype Get##name##Settings() const; \
  void Set##name##Settings(const cpptype&); \
  void Reset##name##Settings();
  OPENKNEEBOARD_SETTINGS_SECTIONS
#undef IT

  void SaveSettings();

  void PostUserAction(UserAction action);

 private:
  DXResources mDXResources;
  ProfileSettings mProfiles {ProfileSettings::Load()};
  Settings mSettings {Settings::Load(mProfiles.mActiveProfile)};

  uint8_t mFirstViewIndex = 0;
  uint8_t mInputViewIndex = 0;
  std::array<std::shared_ptr<KneeboardView>, 2> mViews;

  std::unique_ptr<GamesList> mGamesList;
  std::unique_ptr<TabsList> mTabsList;
  std::unique_ptr<InterprocessRenderer> mInterprocessRenderer;
  // Initalization and destruction order must match as they both use
  // SetWindowLongPtr
  std::unique_ptr<TabletInputAdapter> mTabletInput;
  std::unique_ptr<DirectInputAdapter> mDirectInput;

  std::unique_ptr<GameEventServer> mGameEventServer;
  winrt::Windows::Foundation::IAsyncAction mGameEventWorker;
  std::jthread mOpenVRThread;
  std::optional<RunningGame> mCurrentGame;

  void OnGameChangedEvent(DWORD processID, std::shared_ptr<GameInstance> game);
  void OnGameEvent(const GameEvent& ev) noexcept;

  void StartOpenVRThread();

  void SetFirstViewIndex(uint8_t index);
};

}// namespace OpenKneeboard
