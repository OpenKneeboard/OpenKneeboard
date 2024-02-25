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

class KneeboardState;
class OTDIPCClient;
class TabletInputDevice;
class UserInputDevice;
class WintabTablet;

enum class UserAction;

struct CursorEvent;
struct TabletInfo;
struct TabletState;

class TabletInputAdapter final
  : private EventReceiver,
    public std::enable_shared_from_this<TabletInputAdapter> {
 public:
  static std::shared_ptr<TabletInputAdapter>
  Create(HWND, KneeboardState*, const TabletSettings&);
  ~TabletInputAdapter();

  TabletSettings GetSettings() const;
  void LoadSettings(const TabletSettings&);
  Event<> evSettingsChangedEvent;

  WintabMode GetWintabMode() const;
  winrt::Windows::Foundation::IAsyncAction SetWintabMode(WintabMode);

  bool HaveAnyTablet() const;

  bool IsOTDIPCEnabled() const;
  void SetIsOTDIPCEnabled(bool);

  std::vector<std::shared_ptr<UserInputDevice>> GetDevices() const;

  Event<UserAction> evUserActionEvent;
  Event<std::shared_ptr<UserInputDevice>> evDeviceConnectedEvent;

  TabletInputAdapter() = delete;

 private:
  OPENKNEEBOARD_TraceLoggingScope("TabletInputAdapter");

  TabletInputAdapter(HWND, KneeboardState*, const TabletSettings&);
  void Init();
  void StartWintab();
  void StopWintab();
  void StartOTDIPC();
  void StopOTDIPC();

  winrt::apartment_context mUIThread;

  KneeboardState* mKneeboard;
  TabletSettings mSettings;
  std::unordered_map<std::string, uint32_t> mAuxButtons;

  void OnTabletInput(
    const TabletInfo& tablet,
    const TabletState& state,
    const std::shared_ptr<TabletInputDevice>&);

  std::shared_ptr<TabletInputDevice> CreateDevice(
    const std::string& name,
    const std::string& id);
  void LoadSettings(
    const TabletSettings&,
    const std::shared_ptr<TabletInputDevice>& device);

  ///// OpenTabletDriver /////

  winrt::fire_and_forget OnOTDInput(const std::string& id, const TabletState&);
  winrt::fire_and_forget OnOTDDevice(const TabletInfo&);
  std::shared_ptr<TabletInputDevice> GetOTDDevice(const std::string& id);

  std::shared_ptr<OTDIPCClient> mOTDIPC;
  std::unordered_map<std::string, std::shared_ptr<TabletInputDevice>>
    mOTDDevices;

  ///// Wintab /////

  void OnWintabMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
  static LRESULT SubclassProc(
    HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR uIdSubclass,
    DWORD_PTR dwRefData);

  HWND mWindow {};
  std::unique_ptr<WintabTablet> mWintabTablet;
  std::shared_ptr<TabletInputDevice> mWintabDevice;
  UINT_PTR mSubclassID {};
  static UINT_PTR gNextSubclassID;
};

}// namespace OpenKneeboard
