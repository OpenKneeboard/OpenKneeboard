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

#include <OTD-IPC/DeviceInfo.h>
#include <OTD-IPC/MessageType.h>
#include <OTD-IPC/NamedPipePath.h>
#include <OTD-IPC/State.h>
#include <OpenKneeboard/OTDIPCClient.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/final_release_deleter.h>
#include <OpenKneeboard/scope_guard.h>

#include <ranges>

using namespace OTDIPC::Messages;

namespace OpenKneeboard {

std::shared_ptr<OTDIPCClient> OTDIPCClient::Create() {
  auto ret = shared_with_final_release(new OTDIPCClient());
  ret->mRunner = ret->Run();
  return ret;
}

OTDIPCClient::OTDIPCClient() {
}

OTDIPCClient::~OTDIPCClient() = default;

winrt::fire_and_forget OTDIPCClient::final_release(
  std::unique_ptr<OTDIPCClient> self) {
  try {
    self->mRunner.Cancel();
    co_await self->mRunner;
  } catch (const winrt::hresult_canceled&) {
  }
}

winrt::Windows::Foundation::IAsyncAction OTDIPCClient::Run() {
  auto weakThis = weak_from_this();
  dprint("Starting OTD-IPC client");
  const scope_guard exitMessage([]() {
    dprintf(
      "Tearing down OTD-IPC client with {} uncaught exceptions",
      std::uncaught_exceptions());
  });
  auto cancelled = co_await winrt::get_cancellation_token();
  try {
    while (true) {
      co_await this->RunSingle(weakThis);
      co_await winrt::resume_after(std::chrono::seconds(1));
    }
  } catch (const winrt::hresult_canceled&) {
    dprint("OTD-IPC coroutine cancelled, assuming clean shutdown");
    co_return;
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

winrt::Windows::Foundation::IAsyncAction OTDIPCClient::RunSingle(
  const std::weak_ptr<OTDIPCClient>& weakThis) {
  auto cancelled = co_await winrt::get_cancellation_token();

  winrt::file_handle connection {CreateFileW(
    OTDIPC::NamedPipePathW,
    GENERIC_READ,
    0,
    nullptr,
    OPEN_EXISTING,
    FILE_FLAG_OVERLAPPED,
    NULL)};
  if (!connection) {
    co_return;
  }

  dprint("Connected to OTD-IPC server");
  const scope_guard exitMessage([]() {
    dprintf(
      "Tearing down OTD-IPC connection with {} uncaught exceptions",
      std::uncaught_exceptions());
  });

  winrt::handle event {CreateEventW(nullptr, FALSE, FALSE, nullptr)};
  OVERLAPPED overlapped {.hEvent = event.get()};

  char buffer[1024];
  using namespace OTDIPC::Messages;
  auto header = reinterpret_cast<const Header* const>(buffer);
  static_assert(sizeof(buffer) >= sizeof(DeviceInfo));
  static_assert(sizeof(buffer) >= sizeof(State));

  while (true) {
    if (cancelled()) {
      dprint("OTD-IPC task cancelled.");
      co_return;
    }

    DWORD bytesRead {};

    const auto readFileSuccess = ReadFile(
      connection.get(), buffer, sizeof(buffer), &bytesRead, &overlapped);
    const auto readFileError = GetLastError();
    if ((!readFileSuccess) && readFileError != ERROR_IO_PENDING) {
      dprintf("OTD-IPC ReadFile failed: {}", readFileError);
      co_return;
    }
    if (readFileError == ERROR_IO_PENDING) {
      auto keepAlive = weakThis.lock();
      if (!keepAlive) {
        co_return;
      }

      bool haveEvent = false;
      while (!mTabletsToTimeout.empty()) {
        const auto first = mTabletsToTimeout.begin();
        auto firstID = first->first;
        auto firstTimeout = first->second;

        for (const auto [id, timeout]: mTabletsToTimeout) {
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
        co_await winrt::resume_on_signal(event.get());
      }
      if (!GetOverlappedResult(
            connection.get(), &overlapped, &bytesRead, TRUE)) {
        dprintf("OTD-IPC GetOverlappedResult() failed: {}", GetLastError());
      }
    }

    if (bytesRead < sizeof(Header)) {
      dprintf("OTD-IPC packet smaller than header: {}", bytesRead);
      co_return;
    }

    if (bytesRead < header->size) {
      dprintf(
        "OTD-IPC packet smaller than expected packet size: {} < {}",
        bytesRead,
        header->size);
      co_return;
    }

    auto self = weakThis.lock();
    if (!self) {
      co_return;
    }
    self->ProcessMessage(header);
  }
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

  dprintf(
    "Received OTD-IPC device: '{}' - {}", info.mDeviceName, info.mDeviceID);

  mTablets[info.mDeviceID].mDevice = info;
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
    dprintf("Received first packet for OTD-IPC device {}", deviceID);
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

  evTabletInputEvent.Emit(deviceID, state);
}

}// namespace OpenKneeboard
