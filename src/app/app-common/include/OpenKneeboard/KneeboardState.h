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
#include <OpenKneeboard/Settings.h>
#include <d2d1.h>

#include <thread>
#include <memory>
#include <vector>

namespace OpenKneeboard {

enum class UserAction;
struct CursorEvent;
struct GameEvent;
class DirectInputAdapter;
class GamesList;
class InterprocessRenderer;
class TabState;
class TabletInputAdapter;
class TabsList;
class UserInputDevice;

class KneeboardState final : private EventReceiver {
 public:
  KneeboardState() = delete;
  KneeboardState(HWND mainWindow, const DXResources&);
  ~KneeboardState();

  std::vector<std::shared_ptr<TabState>> GetTabs() const;
  void SetTabs(const std::vector<std::shared_ptr<TabState>>& tabs);
  void InsertTab(uint8_t index, const std::shared_ptr<TabState>& tab);
  void AppendTab(const std::shared_ptr<TabState>& tab);
  void RemoveTab(uint8_t index);

  std::shared_ptr<TabState> GetCurrentTab() const;
  uint8_t GetTabIndex() const;
  void SetTabIndex(uint8_t);
  void SetTabID(uint64_t);
  void PreviousTab();
  void NextTab();

  void NextPage();
  void PreviousPage();

  const D2D1_SIZE_U& GetCanvasSize() const;
  const D2D1_RECT_F& GetHeaderRenderRect() const;
  const D2D1_RECT_F& GetContentRenderRect() const;
  /// ContentRenderRect may be scaled; this is the 'real' size.
  const D2D1_SIZE_U& GetContentNativeSize() const;

  bool HaveCursor() const;
  D2D1_POINT_2F GetCursorPoint() const;
  D2D1_POINT_2F GetCursorCanvasPoint() const;
  D2D1_POINT_2F GetCursorCanvasPoint(const D2D1_POINT_2F&) const;

  Event<uint8_t> evCurrentTabChangedEvent;
  Event<> evNeedsRepaintEvent;
  Event<const CursorEvent&> evCursorEvent;
  Event<> evFrameTimerEvent;
  Event<> evTabsChangedEvent;

  std::vector<std::shared_ptr<UserInputDevice>> GetInputDevices() const;  

  GamesList* GetGamesList() const;

  void SaveSettings();

 private:
  HWND mMainWindow;
  DXResources mDXResources;
  Settings mSettings {Settings::Load()};

  std::vector<std::shared_ptr<TabState>> mTabs;
  std::shared_ptr<TabState> mCurrentTab;

  D2D1_SIZE_U mCanvasSize;
  D2D1_SIZE_U mContentNativeSize;
  D2D1_RECT_F mHeaderRenderRect;
  D2D1_RECT_F mContentRenderRect;

  bool mHaveCursor = false;
  D2D1_POINT_2F mCursorPoint;

  std::unique_ptr<GamesList> mGamesList;
  std::unique_ptr<TabsList> mTabsList;
  std::unique_ptr<InterprocessRenderer> mInterprocessRenderer;
  std::unique_ptr<DirectInputAdapter> mDirectInput;
  std::unique_ptr<TabletInputAdapter> mTabletInput;

  std::jthread mGameEventThread;
  std::jthread mOpenVRThread;

  void UpdateLayout();

  void OnCursorEvent(const CursorEvent& ev);
  void OnUserAction(UserAction);
  void OnGameEvent(const GameEvent&);
};

}// namespace OpenKneeboard
