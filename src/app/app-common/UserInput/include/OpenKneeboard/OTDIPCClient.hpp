// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
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

namespace OTDIPC::Messages {
struct Header;
struct DeviceInfo;
struct State;
};// namespace OTDIPC::Messages

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

  std::optional<TabletState> GetState(const std::string& id) const;
  std::optional<TabletInfo> GetTablet(const std::string& id) const;
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
  void ProcessMessage(const OTDIPC::Messages::Header* const);
  void ProcessMessage(const OTDIPC::Messages::DeviceInfo* const);
  void ProcessMessage(const OTDIPC::Messages::State* const);
  void TimeoutTablet(const std::string& id);

  std::optional<task<void>> mRunner;

  std::stop_source mStopper;

  struct Tablet {
    TabletInfo mDevice;
    std::optional<TabletState> mState;
  };
  std::unordered_map<std::string, Tablet> mTablets;

  using TimeoutClock = std::chrono::steady_clock;
  /* Tablets that do not support proximity data.
   *
   * We just consider them inactive once we stop receiving packets
   * for a while.
   */
  std::unordered_map<std::string, TimeoutClock::time_point> mTabletsToTimeout;
};

}// namespace OpenKneeboard
