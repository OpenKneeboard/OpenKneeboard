// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/IHasDisposeAsync.hpp>
#include <OpenKneeboard/ProcessShutdownBlock.hpp>
#include <OpenKneeboard/TabletInfo.hpp>
#include <OpenKneeboard/TabletState.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <shims/winrt/base.h>

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.h>

#include <chrono>
#include <memory>
#include <unordered_map>

#include <OTD-IPC/DebugMessage.hpp>
#include <OTD-IPC/DeviceInfo.hpp>
#include <OTD-IPC/Header.hpp>
#include <OTD-IPC/State.hpp>

namespace OpenKneeboard {

/**
 * OpenTabletDriver-IPC Tablet
 *
 * https://github.com/OpenKneeboard/OTD-IPC
 */
class OTDIPCClient final : public std::enable_shared_from_this<OTDIPCClient>,
                           public IHasDisposeAsync {
 public:
  static std::shared_ptr<OTDIPCClient> Create();
  ~OTDIPCClient();
  task<void> DisposeAsync() noexcept override;

  std::optional<TabletState> GetState(const std::string& persistentID) const;
  std::optional<TabletInfo> GetTablet(const std::string& persistentID) const;
  std::vector<TabletInfo> GetTablets() const;

  Event<TabletInfo> evDeviceInfoReceivedEvent;
  Event<std::string, TabletState> evTabletInputEvent;

 private:
  DisposalState mDisposal;
  ProcessShutdownBlock mShutdownBlock;
  winrt::apartment_context mUIThread;

  DispatcherQueueController mDQC {nullptr};

  OTDIPCClient();
  task<void> Run();
  task<void> RunSingle();

  OpenKneeboard::fire_and_forget EnqueueMessage(std::string message);
  void ProcessMessage(const OTDIPC::Messages::Header&);
  void ProcessMessage(const OTDIPC::Messages::DeviceInfo&);
  void ProcessMessage(const OTDIPC::Messages::State&);
  void ProcessMessage(const OTDIPC::Messages::DebugMessage&);
  void TimeoutTablet(uint32_t id);
  void TimeoutTablets();

  std::optional<task<void>> mRunner;

  std::stop_source mStopper;

  struct Tablet {
    TabletInfo mDevice;
    std::optional<TabletState> mState;
  };
  std::unordered_map<uint32_t, Tablet> mTablets;
  std::unordered_map<std::string, uint32_t> mTabletIDs;

  using TimeoutClock = std::chrono::steady_clock;
  /* Tablets that do not support proximity data.
   *
   * We just consider them inactive once we stop receiving packets
   * for a while.
   */
  std::unordered_map<uint32_t, TimeoutClock::time_point> mTabletsToTimeout;

  template <
    class T,
    class TTablet =
      std::conditional_t<std::is_const_v<T>, const Tablet, Tablet>>
  std::optional<TTablet*> GetTabletFromPersistentID(
    this T& self,
    const std::string& persistentID);
};

}// namespace OpenKneeboard
