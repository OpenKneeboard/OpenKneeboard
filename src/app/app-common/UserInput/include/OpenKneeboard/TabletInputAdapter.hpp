// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Elevation.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/IHasDisposeAsync.hpp>
#include <OpenKneeboard/TabletSettings.hpp>

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
    public std::enable_shared_from_this<TabletInputAdapter>,
    public IHasDisposeAsync {
 public:
  static std::shared_ptr<TabletInputAdapter>
  Create(HWND, KneeboardState*, const TabletSettings&);
  ~TabletInputAdapter() override;

  task<void> DisposeAsync() noexcept override;

  TabletSettings GetSettings() const;
  void LoadSettings(const TabletSettings&);
  Event<> evSettingsChangedEvent;

  bool HaveAnyTablet() const;

  bool IsOTDIPCEnabled() const;
  task<void> SetIsOTDIPCEnabled(bool);

  std::vector<std::shared_ptr<UserInputDevice>> GetDevices() const;
  std::vector<TabletInfo> GetTabletInfo() const;

  Event<UserAction> evUserActionEvent;
  Event<std::shared_ptr<UserInputDevice>> evDeviceConnectedEvent;

  TabletInputAdapter() = delete;

 private:
  TabletInputAdapter(KneeboardState*, const TabletSettings&);
  void Init();
  void StartOTDIPC();
  task<void> StopOTDIPC();

  winrt::apartment_context mUIThread;
  DisposalState mDisposal;

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

  OpenKneeboard::fire_and_forget OnOTDInput(std::string id, TabletState);
  OpenKneeboard::fire_and_forget OnOTDDevice(TabletInfo);
  std::shared_ptr<TabletInputDevice> GetOTDDevice(const std::string& id);

  std::shared_ptr<OTDIPCClient> mOTDIPC;
  std::unordered_map<std::string, std::shared_ptr<TabletInputDevice>>
    mOTDDevices;
};

}// namespace OpenKneeboard
