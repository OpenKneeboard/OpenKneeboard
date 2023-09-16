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
#include <OpenKneeboard/TabletInfo.h>
#include <OpenKneeboard/TabletState.h>
#include <OpenKneeboard/Win32.h>

#include <shims/winrt/base.h>

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
class OTDIPCClient final : public std::enable_shared_from_this<OTDIPCClient> {
 public:
  static std::shared_ptr<OTDIPCClient> Create();
  ~OTDIPCClient();
  static winrt::fire_and_forget final_release(std::unique_ptr<OTDIPCClient>);

  std::optional<TabletState> GetState(const std::string& id) const;
  std::optional<TabletInfo> GetTablet(const std::string& id) const;
  std::vector<TabletInfo> GetTablets() const;

  Event<TabletInfo> evDeviceInfoReceivedEvent;
  Event<std::string, TabletState> evTabletInputEvent;

 private:
  OTDIPCClient();
  winrt::Windows::Foundation::IAsyncAction Run();
  winrt::Windows::Foundation::IAsyncAction RunSingle(
    const std::weak_ptr<OTDIPCClient>&);

  void ProcessMessage(const OTDIPC::Messages::Header* const);
  void ProcessMessage(const OTDIPC::Messages::DeviceInfo* const);
  void ProcessMessage(const OTDIPC::Messages::State* const);
  void TimeoutTablet(const std::string& id);

  winrt::Windows::Foundation::IAsyncAction mRunner;
  winrt::handle mCompletionHandle {
    Win32::CreateEventW(nullptr, TRUE, FALSE, nullptr)};

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
