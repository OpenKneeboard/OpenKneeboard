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

#include <OpenKneeboard/OTDIPCClient.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/final_release_deleter.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/task/resume_after.hpp>

#include <wil/cppwinrt.h>

#include <ranges>

#include <wil/cppwinrt_helpers.h>

#include <OTD-IPC/DeviceInfo.h>
#include <OTD-IPC/MessageType.h>
#include <OTD-IPC/NamedPipePath.h>
#include <OTD-IPC/State.h>

using namespace OTDIPC::Messages;

namespace OpenKneeboard {

std::shared_ptr<OTDIPCClient> OTDIPCClient::Create() {
  auto ret = shared_with_final_release(new OTDIPCClient());
  ret->mRunner = ret->Run();
  return ret;
}

OTDIPCClient::OTDIPCClient() {
  dprint("{}", __FUNCTION__);
  mDQC = DispatcherQueueController::CreateOnDedicatedThread();
}

OTDIPCClient::~OTDIPCClient() {
  dprint("{}", __FUNCTION__);
}

OpenKneeboard::fire_and_forget OTDIPCClient::final_release(
  std::unique_ptr<OTDIPCClient> self) {
  dprint("Requesting OTDIPCClient stop");
  self->mStopper.request_stop();
  dprint("Waiting for OTDIPCClient completion handle");
  co_await winrt::resume_on_signal(self->mCompletionHandle.get());
  dprint("Waiting for OTDIPCClient coro");
  co_await std::move(self->mRunner).value();
  dprint("Shutting down DQC");
  co_await self->mDQC.ShutdownQueueAsync();
  dprint("Destroying OTDIPCClient");
}

task<void> OTDIPCClient::Run() {
  const scope_exit markCompletion([handle = mCompletionHandle.get()]() {
    dprint("Setting OTDIPC completion handle");
    SetEvent(handle);
  });
  dprint("Starting OTD-IPC client");
  const scope_exit exitMessage([]() {
    dprint(
      "Tearing down OTD-IPC client with {} uncaught exceptions",
      std::uncaught_exceptions());
  });
  auto workThread = mDQC.DispatcherQueue();
  co_await wil::resume_foreground(workThread);
  SetThreadDescription(GetCurrentThread(), L"OTD-IPC Client Thread");

  while (!mStopper.stop_requested()) {
    co_await this->RunSingle();
    co_await wil::resume_foreground(workThread);
    co_await resume_after(std::chrono::seconds(1), mStopper.get_token());
  }
}

void OTDIPCClient::TimeoutTablet(const std::string& id) {
  mTabletsToTimeout.erase(id);

  auto it = mTablets.find(id);
  if (it == mTablets.end()) {
    return;
  }

  auto& state = it->second.mState;
  if (!state) {
    return;
  }
  state->mIsActive = false;
  evTabletInputEvent.Emit(id, *state);
}

task<void> OTDIPCClient::RunSingle() {
  const auto connection = Win32::or_default::CreateFile(
    OTDIPC::NamedPipePathW,
    GENERIC_READ,
    0,
    nullptr,
    OPEN_EXISTING,
    FILE_FLAG_OVERLAPPED,
    NULL);
  if (!connection) {
    co_return;
  }

  dprint("Connected to OTD-IPC server");
  const scope_exit exitMessage([]() {
    dprint(
      "Tearing down OTD-IPC connection with {} uncaught exceptions",
      std::uncaught_exceptions());
  });

  const auto event
    = Win32::or_throw::CreateEvent(nullptr, FALSE, FALSE, nullptr);
  OVERLAPPED overlapped {.hEvent = event.get()};

  char buffer[1024];
  using namespace OTDIPC::Messages;
  auto header = reinterpret_cast<const Header* const>(buffer);
  static_assert(sizeof(buffer) >= sizeof(DeviceInfo));
  static_assert(sizeof(buffer) >= sizeof(State));

  while (true) {
    if (mStopper.stop_requested()) {
      dprint("OTD-IPC task cancelled.");
      co_return;
    }

    DWORD bytesRead {};

    const auto readFileSuccess = ReadFile(
      connection.get(), buffer, sizeof(buffer), &bytesRead, &overlapped);
    const auto readFileError = GetLastError();
    if ((!readFileSuccess) && readFileError != ERROR_IO_PENDING) {
      dprint("OTD-IPC ReadFile failed: {}", readFileError);
      co_return;
    }
    if (readFileError == ERROR_IO_PENDING) {
      bool haveEvent = false;
      while (!mTabletsToTimeout.empty()) {
        const auto first = mTabletsToTimeout.begin();
        auto firstID = first->first;
        auto firstTimeout = first->second;

        for (const auto& [id, timeout]: mTabletsToTimeout) {
          if (timeout < firstTimeout) {
            firstID = id;
            firstTimeout = timeout;
          }
        }

        const auto now = TimeoutClock::now();
        if (firstTimeout < now) {
          TimeoutTablet(firstID);
          continue;
        }

        if (co_await winrt::resume_on_signal(
              event.get(),
              std::chrono::duration_cast<std::chrono::milliseconds>(
                firstTimeout - now))) {
          haveEvent = true;
          break;
        }

        TimeoutTablet(firstID);
      }

      if (!haveEvent) {
        auto stop = mStopper.get_token();
        co_await resume_on_signal(stop, event.get());
        if (stop.stop_requested()) {
          co_return;
        }
      }
      if (!GetOverlappedResult(
            connection.get(), &overlapped, &bytesRead, TRUE)) {
        dprint("OTD-IPC GetOverlappedResult() failed: {}", GetLastError());
      }
    }

    if (bytesRead < sizeof(Header)) {
      dprint("OTD-IPC packet smaller than header: {}", bytesRead);
      co_return;
    }

    if (bytesRead < header->size) {
      dprint(
        "OTD-IPC packet smaller than expected packet size: {} < {}",
        bytesRead,
        header->size);
      co_return;
    }

    this->EnqueueMessage({buffer, header->size});
  }
}

OpenKneeboard::fire_and_forget OTDIPCClient::EnqueueMessage(
  std::string message) {
  auto weakThis = weak_from_this();
  co_await mUIThread;
  auto self = weakThis.lock();
  if (!self) {
    co_return;
  }
  this->ProcessMessage(
    reinterpret_cast<const OTDIPC::Messages::Header*>(message.data()));
}

std::optional<TabletState> OTDIPCClient::GetState(const std::string& id) const {
  auto it = mTablets.find(id);
  if (it == mTablets.end()) {
    return {};
  }
  return it->second.mState;
}

std::optional<TabletInfo> OTDIPCClient::GetTablet(const std::string& id) const {
  auto it = mTablets.find(id);
  if (it == mTablets.end()) {
    return {};
  }
  return it->second.mDevice;
}

std::vector<TabletInfo> OTDIPCClient::GetTablets() const {
  std::vector<TabletInfo> ret;
  for (const auto& tablet: mTablets) {
    ret.push_back(tablet.second.mDevice);
  }
  return ret;
}

void OTDIPCClient::ProcessMessage(
  const OTDIPC::Messages::Header* const header) {
  switch (header->messageType) {
    case MessageType::DeviceInfo:
      this->ProcessMessage(reinterpret_cast<const DeviceInfo* const>(header));
      return;
    case MessageType::State:
      this->ProcessMessage(reinterpret_cast<const State* const>(header));
      return;
    case MessageType::Ping:
      // nothing to do
      return;
  }
}

static std::string MakeDeviceID(const Header* const header) {
  return std::format(
    "otdipc-vidpid:///{:04x}/{:04x}", header->vid, header->pid);
}

void OTDIPCClient::ProcessMessage(
  const OTDIPC::Messages::DeviceInfo* const msg) {
  if (msg->size < sizeof(DeviceInfo)) {
    return;
  }

  TabletInfo info {
    .mMaxX = msg->maxX,
    .mMaxY = msg->maxY,
    .mMaxPressure = msg->maxPressure,
    .mDeviceName = winrt::to_string(msg->name),
    .mDeviceID = MakeDeviceID(msg),
  };

  dprint(
    "Received OTD-IPC device: '{}' - {}", info.mDeviceName, info.mDeviceID);

  mTablets[info.mDeviceID].mDevice = info;
  evDeviceInfoReceivedEvent.Emit(info);
}

void OTDIPCClient::ProcessMessage(const OTDIPC::Messages::State* const msg) {
  if (msg->size < sizeof(State)) {
    return;
  }

  const auto deviceID = MakeDeviceID(msg);
  if (!mTablets.contains(deviceID)) {
    return;
  }

  auto& tablet = mTablets[deviceID];
  if (!tablet.mState) {
    dprint("Received first packet for OTD-IPC device {}", deviceID);
    tablet.mState = TabletState {};
  }
  auto& state = tablet.mState.value();

  if (msg->positionValid) {
    state.mX = msg->x;
    state.mY = msg->y;
  }

  if (msg->pressureValid) {
    state.mPressure = msg->pressure;
    if (state.mPressure > 0) {
      state.mPenButtons |= 1;
    } else {
      state.mPenButtons &= ~1;
    }
  }

  if (msg->penButtonsValid) {
    // Preserve tip button
    state.mPenButtons &= 1;
    state.mPenButtons |= (msg->penButtons << 1);
  }

  if (msg->auxButtonsValid) {
    state.mAuxButtons = msg->auxButtons;
  }

  if (msg->proximityValid) {
    // e.g. Wacom
    state.mIsActive = msg->nearProximity;
  } else if (msg->positionValid) {
    // e.g. Huion does not have proximity
    state.mIsActive = true;
    mTabletsToTimeout[deviceID]
      = TimeoutClock::now() + std::chrono::milliseconds(100);
  }

  OPENKNEEBOARD_TraceLoggingScope("OTDIPCClient::evTabletInputEvent");
  evTabletInputEvent.Emit(deviceID, state);
}

}// namespace OpenKneeboard
