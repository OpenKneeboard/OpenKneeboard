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

#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/TabletSettings.h>
#include <Windows.h>

#include <memory>
#include <tuple>
#include <vector>

namespace OpenKneeboard {

enum class UserAction;
class UserInputDevice;
struct CursorEvent;
class KneeboardState;
class WintabTablet;
class TabletInputDevice;

class TabletInputAdapter final : private EventReceiver {
 public:
  TabletInputAdapter() = delete;
  TabletInputAdapter(HWND, KneeboardState*, const TabletSettings&);
  ~TabletInputAdapter();

  TabletSettings GetSettings() const;
  void LoadSettings(const TabletSettings&);
  Event<> evSettingsChangedEvent;

  std::vector<std::shared_ptr<UserInputDevice>> GetDevices() const;

  Event<UserAction> evUserActionEvent;

 private:
  HWND mWindow;
  KneeboardState* mKneeboard;
  TabletSettings mInitialSettings;
  std::unique_ptr<WintabTablet> mTablet;
  std::shared_ptr<TabletInputDevice> mDevice;
  WNDPROC mPreviousWndProc;
  uint16_t mAuxButtons = 0;

  void ProcessTabletMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

  static LRESULT CALLBACK WindowProc(
    _In_ HWND hwnd,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam);
};

}// namespace OpenKneeboard
